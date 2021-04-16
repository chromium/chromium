// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <windows.h>
#include <wintrust.h>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/win/pe_image_reader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Gt;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;

namespace base {
namespace win {

struct TestData {
  const char* filename;
  PeImageReader::WordSize word_size;
  WORD machine_identifier;
  WORD optional_header_size;
  size_t number_of_sections;
  size_t number_of_debug_entries;
};

// A test fixture parameterized on test data containing the name of a PE image
// to parse and the expected values to be read from it. The file is read from
// the src/base/test/data/pe_image_reader directory.
class PeImageReaderTest : public testing::TestWithParam<const TestData*> {
 protected:
  PeImageReaderTest() : expected_data_(GetParam()) {}

  void SetUp() override {
    ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &data_file_path_));
    data_file_path_ = data_file_path_.AppendASCII("pe_image_reader");
    data_file_path_ = data_file_path_.AppendASCII(expected_data_->filename);

    ASSERT_TRUE(data_file_.Initialize(data_file_path_));

    ASSERT_TRUE(
        image_reader_.Initialize(data_file_.data(), data_file_.length()));
  }

  const TestData* expected_data_;
  FilePath data_file_path_;
  MemoryMappedFile data_file_;
  PeImageReader image_reader_;
};

TEST_P(PeImageReaderTest, GetWordSize) {
  EXPECT_EQ(expected_data_->word_size, image_reader_.GetWordSize());
}

TEST_P(PeImageReaderTest, GetDosHeader) {
  const IMAGE_DOS_HEADER* dos_header = image_reader_.GetDosHeader();
  ASSERT_NE(nullptr, dos_header);
  EXPECT_EQ(IMAGE_DOS_SIGNATURE, dos_header->e_magic);
}

TEST_P(PeImageReaderTest, GetCoffFileHeader) {
  const IMAGE_FILE_HEADER* file_header = image_reader_.GetCoffFileHeader();
  ASSERT_NE(nullptr, file_header);
  EXPECT_EQ(expected_data_->machine_identifier, file_header->Machine);
  EXPECT_EQ(expected_data_->optional_header_size,
            file_header->SizeOfOptionalHeader);
}

TEST_P(PeImageReaderTest, GetOptionalHeaderData) {
  size_t optional_header_size = 0;
  const uint8_t* optional_header_data =
      image_reader_.GetOptionalHeaderData(&optional_header_size);
  ASSERT_NE(nullptr, optional_header_data);
  EXPECT_EQ(expected_data_->optional_header_size, optional_header_size);
}

TEST_P(PeImageReaderTest, GetNumberOfSections) {
  EXPECT_EQ(expected_data_->number_of_sections,
            image_reader_.GetNumberOfSections());
}

TEST_P(PeImageReaderTest, GetSectionHeaderAt) {
  size_t number_of_sections = image_reader_.GetNumberOfSections();
  for (size_t i = 0; i < number_of_sections; ++i) {
    const IMAGE_SECTION_HEADER* section_header =
        image_reader_.GetSectionHeaderAt(i);
    ASSERT_NE(nullptr, section_header);
  }
}

TEST_P(PeImageReaderTest, InitializeFailTruncatedFile) {
  // Compute the size of all headers through the section headers.
  const IMAGE_SECTION_HEADER* last_section_header =
      image_reader_.GetSectionHeaderAt(image_reader_.GetNumberOfSections() - 1);
  const uint8_t* headers_end =
      reinterpret_cast<const uint8_t*>(last_section_header) +
      sizeof(*last_section_header);
  size_t header_size = headers_end - data_file_.data();
  PeImageReader short_reader;

  // Initialize should succeed when all headers are present.
  EXPECT_TRUE(short_reader.Initialize(data_file_.data(), header_size));

  // But fail if anything is missing.
  for (size_t i = 0; i < header_size; ++i) {
    EXPECT_FALSE(short_reader.Initialize(data_file_.data(), i));
  }
}

TEST_P(PeImageReaderTest, GetExportSection) {
  size_t section_size = 0;
  const uint8_t* export_section = image_reader_.GetExportSection(&section_size);
  ASSERT_NE(nullptr, export_section);
  EXPECT_NE(0U, section_size);
}

TEST_P(PeImageReaderTest, GetNumberOfDebugEntries) {
  EXPECT_EQ(expected_data_->number_of_debug_entries,
            image_reader_.GetNumberOfDebugEntries());
}

TEST_P(PeImageReaderTest, GetDebugEntry) {
  size_t number_of_debug_entries = image_reader_.GetNumberOfDebugEntries();
  for (size_t i = 0; i < number_of_debug_entries; ++i) {
    const uint8_t* raw_data = nullptr;
    size_t raw_data_size = 0;
    const IMAGE_DEBUG_DIRECTORY* entry =
        image_reader_.GetDebugEntry(i, &raw_data, &raw_data_size);
    EXPECT_NE(nullptr, entry);
    EXPECT_NE(nullptr, raw_data);
    EXPECT_NE(0U, raw_data_size);
  }
}

namespace {

const TestData kTestData[] = {
    {
        "module_with_exports_x86.dll",
        PeImageReader::WORD_SIZE_32,
        IMAGE_FILE_MACHINE_I386,
        sizeof(IMAGE_OPTIONAL_HEADER32),
        4,
        1,
    },
    {
        "module_with_exports_x64.dll",
        PeImageReader::WORD_SIZE_64,
        IMAGE_FILE_MACHINE_AMD64,
        sizeof(IMAGE_OPTIONAL_HEADER64),
        5,
        1,
    },
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(WordSize32,
                         PeImageReaderTest,
                         testing::Values(&kTestData[0]));
INSTANTIATE_TEST_SUITE_P(WordSize64,
                         PeImageReaderTest,
                         testing::Values(&kTestData[1]));

// An object exposing a PeImageReader::EnumCertificatesCallback that invokes a
// virtual OnCertificate() method. This method is suitable for mocking in tests.
class CertificateReceiver {
 public:
  void* AsContext() { return this; }
  static bool OnCertificateCallback(uint16_t revision,
                                    uint16_t certificate_type,
                                    const uint8_t* certificate_data,
                                    size_t certificate_data_size,
                                    void* context) {
    return reinterpret_cast<CertificateReceiver*>(context)->OnCertificate(
        revision, certificate_type, certificate_data, certificate_data_size);
  }

 protected:
  CertificateReceiver() = default;
  virtual ~CertificateReceiver() = default;
  virtual bool OnCertificate(uint16_t revision,
                             uint16_t certificate_type,
                             const uint8_t* certificate_data,
                             size_t certificate_data_size) = 0;
};

class MockCertificateReceiver : public CertificateReceiver {
 public:
  MockCertificateReceiver() = default;
  MOCK_METHOD4(OnCertificate, bool(uint16_t, uint16_t, const uint8_t*, size_t));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCertificateReceiver);
};

struct CertificateTestData {
  const char* filename;
  int num_signers;
};

// A test fixture parameterized on test data containing the name of a PE image
// to parse and the expected values to be read from it. The file is read from
// the src/chrome/test/data/safe_browsing/download_protection directory.
class PeImageReaderCertificateTest
    : public testing::TestWithParam<const CertificateTestData*> {
 protected:
  PeImageReaderCertificateTest() : expected_data_(GetParam()) {}

  void SetUp() override {
    ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &data_file_path_));
    data_file_path_ = data_file_path_.AppendASCII("pe_image_reader");
    data_file_path_ = data_file_path_.AppendASCII(expected_data_->filename);
    ASSERT_TRUE(data_file_.Initialize(data_file_path_));
    ASSERT_TRUE(
        image_reader_.Initialize(data_file_.data(), data_file_.length()));
  }

  const CertificateTestData* expected_data_;
  FilePath data_file_path_;
  MemoryMappedFile data_file_;
  PeImageReader image_reader_;
};

TEST_P(PeImageReaderCertificateTest, EnumCertificates) {
  StrictMock<MockCertificateReceiver> receiver;
  if (expected_data_->num_signers) {
    EXPECT_CALL(receiver, OnCertificate(WIN_CERT_REVISION_2_0,
                                        WIN_CERT_TYPE_PKCS_SIGNED_DATA,
                                        NotNull(), Gt(0U)))
        .Times(expected_data_->num_signers)
        .WillRepeatedly(Return(true));
  }
  EXPECT_TRUE(image_reader_.EnumCertificates(
      &CertificateReceiver::OnCertificateCallback, receiver.AsContext()));
}

TEST_P(PeImageReaderCertificateTest, AbortEnum) {
  StrictMock<MockCertificateReceiver> receiver;
  if (expected_data_->num_signers) {
    // Return false for the first cert, thereby stopping the enumeration.
    EXPECT_CALL(receiver, OnCertificate(_, _, _, _)).WillOnce(Return(false));
    EXPECT_FALSE(image_reader_.EnumCertificates(
        &CertificateReceiver::OnCertificateCallback, receiver.AsContext()));
  } else {
    // An unsigned file always reports true with no invocations of the callback.
    EXPECT_TRUE(image_reader_.EnumCertificates(
        &CertificateReceiver::OnCertificateCallback, receiver.AsContext()));
  }
}

namespace {

const CertificateTestData kCertificateTestData[] = {
    {
        "signed.exe",
        1,
    },
    {
        "unsigned.exe",
        0,
    },
    {
        "disable_outdated_build_detector.exe",
        1,
    },
    {
        "signed_twice.exe",
        2,
    },
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(SignedExe,
                         PeImageReaderCertificateTest,
                         testing::Values(&kCertificateTestData[0]));
INSTANTIATE_TEST_SUITE_P(UnsignedExe,
                         PeImageReaderCertificateTest,
                         testing::Values(&kCertificateTestData[1]));
INSTANTIATE_TEST_SUITE_P(DisableOutdatedBuildDetectorExe,
                         PeImageReaderCertificateTest,
                         testing::Values(&kCertificateTestData[2]));
INSTANTIATE_TEST_SUITE_P(SignedTwiceExe,
                         PeImageReaderCertificateTest,
                         testing::Values(&kCertificateTestData[3]));

}  // namespace win
}  // namespace base
