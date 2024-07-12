// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_rtp_dump_writer.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <memory>

#include "base/containers/span_reader.h"
#include "base/containers/span_writer.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/numerics/byte_conversions.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/zlib.h"

static const size_t kMinimumRtpHeaderLength = 12;

static std::vector<uint8_t> CreateFakeRtpPacketHeader(
    size_t csrc_count,
    size_t extension_header_count) {
  std::vector<uint8_t> packet_header(
      kMinimumRtpHeaderLength + csrc_count * sizeof(uint32_t) +
      (extension_header_count + 1) * sizeof(uint32_t));

  // First byte format: vvpxcccc, where 'vv' is the version, 'p' is padding, 'x'
  // is the extension bit, 'cccc' is the CSRC count.
  packet_header[0] = 0;
  packet_header[0] |= (0x2 << 6);  // version.
  // The extension bit.
  packet_header[0] |= (extension_header_count > 0 ? (0x1 << 4) : 0);
  packet_header[0] |= (csrc_count & 0xf);

  // Set extension length.
  size_t offset = kMinimumRtpHeaderLength +
                  (csrc_count & 0xf) * sizeof(uint32_t) + sizeof(uint16_t);
  auto writer = base::SpanWriter(base::span(packet_header));
  writer.Skip(offset);
  writer.WriteU16BigEndian(static_cast<uint16_t>(extension_header_count));

  return packet_header;
}

static void FlushTaskRunner(base::SequencedTaskRunner* task_runner) {
  base::RunLoop run_loop;
  task_runner->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

class WebRtcRtpDumpWriterTest : public testing::Test {
 public:
  WebRtcRtpDumpWriterTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        temp_dir_(new base::ScopedTempDir()) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_->CreateUniqueTempDir());

    incoming_dump_path_ = temp_dir_->GetPath().AppendASCII("rtpdump_recv");
    outgoing_dump_path_ = temp_dir_->GetPath().AppendASCII("rtpdump_send");
    writer_ = std::make_unique<WebRtcRtpDumpWriter>(
        incoming_dump_path_, outgoing_dump_path_, 4 * 1024 * 1024,
        base::BindRepeating(&WebRtcRtpDumpWriterTest::OnMaxSizeReached,
                            base::Unretained(this)));
  }

  // Verifies that the dump contains records of |rtp_packet| repeated
  // |packet_count| times.
  void VerifyDumps(size_t incoming_packet_count, size_t outgoing_packet_count) {
    std::string incoming_dump;
    std::string outgoing_dump;

    if (incoming_packet_count) {
      EXPECT_TRUE(base::ReadFileToString(incoming_dump_path_, &incoming_dump));
      EXPECT_TRUE(VerifyCompressedDump(&incoming_dump, incoming_packet_count));
    } else {
      EXPECT_FALSE(base::PathExists(incoming_dump_path_));
    }

    if (outgoing_packet_count) {
      EXPECT_TRUE(base::ReadFileToString(outgoing_dump_path_, &outgoing_dump));
      EXPECT_TRUE(VerifyCompressedDump(&outgoing_dump, outgoing_packet_count));
    } else {
      EXPECT_FALSE(base::PathExists(outgoing_dump_path_));
    }
  }

  MOCK_METHOD2(OnEndDumpDone, void(bool, bool));
  MOCK_METHOD0(OnMaxSizeReached, void(void));

 protected:
  // Verifies the compressed dump file contains the expected number of packets.
  bool VerifyCompressedDump(std::string* dump, size_t expected_packet_count) {
    EXPECT_GT(dump->size(), 0U);

    std::vector<uint8_t> decompressed_dump;
    EXPECT_TRUE(Decompress(dump, &decompressed_dump));

    size_t actual_packet_count = 0;
    EXPECT_TRUE(ReadDecompressedDump(decompressed_dump, &actual_packet_count));
    EXPECT_EQ(expected_packet_count, actual_packet_count);

    return true;
  }

  // Decompresses the |input| into |output|.
  bool Decompress(std::string* input, std::vector<uint8_t>* output) {
    z_stream stream = {0};

    int result = inflateInit2(&stream, 15 + 16);
    EXPECT_EQ(Z_OK, result);

    output->resize(input->size() * 100);

    stream.next_in =
        reinterpret_cast<unsigned char*>(const_cast<char*>(&(*input)[0]));
    stream.avail_in = input->size();
    stream.next_out = &(*output)[0];
    stream.avail_out = output->size();

    result = inflate(&stream, Z_FINISH);
    DCHECK_EQ(Z_STREAM_END, result);
    result = inflateEnd(&stream);
    DCHECK_EQ(Z_OK, result);

    output->resize(output->size() - stream.avail_out);
    return true;
  }

  // Tries to read |dump| as a rtpplay dump file and returns the number of
  // packets found in the dump.
  bool ReadDecompressedDump(base::span<uint8_t> dump, size_t* packet_count) {
    static const char kFirstLine[] = "#!rtpplay1.0 0.0.0.0/0\n";
    static const size_t kDumpFileHeaderSize = 4 * sizeof(uint32_t);

    *packet_count = 0;
    size_t dump_pos = 0;

    // Verifies the first line.
    EXPECT_EQ(memcmp(&dump[0], kFirstLine, std::size(kFirstLine) - 1), 0);

    dump_pos += std::size(kFirstLine) - 1;
    EXPECT_GT(dump.size(), dump_pos);

    // Skips the file header.
    dump_pos += kDumpFileHeaderSize;
    EXPECT_GT(dump.size(), dump_pos);

    // Reads each packet dump.
    while (dump_pos < dump.size()) {
      uint16_t packet_dump_length = 0;
      if (!VerifyPacketDump(dump.subspan(dump_pos), &packet_dump_length)) {
        DVLOG(0) << "Failed to read the packet dump for packet "
                 << *packet_count << ", dump_pos = " << dump_pos
                 << ", dump_length = " << dump.size();
        return false;
      }

      EXPECT_GE(dump.size(), dump_pos + packet_dump_length);
      dump_pos += packet_dump_length;

      (*packet_count)++;
    }
    return true;
  }

  // Tries to read one packet dump starting at |dump| and returns the size of
  // the packet dump.
  bool VerifyPacketDump(base::span<const uint8_t> dump,
                        uint16_t* packet_dump_length) {
    static const size_t kDumpHeaderLength = 8;

    auto reader = base::SpanReader(dump);

    if (!reader.ReadU16BigEndian(*packet_dump_length)) {
      return false;
    }
    if (*packet_dump_length < kDumpHeaderLength + kMinimumRtpHeaderLength) {
      return false;
    }

    if (dump.size() < *packet_dump_length) {
      ADD_FAILURE() << "Failed check 'dump.size() < *packet_dump_length': "
                    << dump.size() << " < " << *packet_dump_length;
      return false;
    }

    uint16_t rtp_packet_length = 0;
    if (!reader.ReadU16BigEndian(rtp_packet_length)) {
      return false;
    }
    if (rtp_packet_length < kMinimumRtpHeaderLength)
      return false;

    // Skips the elapsed time field.
    if (!reader.Skip(sizeof(uint32_t))) {
      return false;
    }

    return IsValidRtpHeader(
        reader.remaining_span().first(*packet_dump_length - kDumpHeaderLength));
  }

  // Returns true if the header is a valid RTP header.
  bool IsValidRtpHeader(base::span<const uint8_t> header) {
    if ((header[0u] & 0xC0u) != 0x80u) {
      return false;
    }

    size_t cc_count = header[0u] & 0x0Fu;
    size_t header_length_without_extn = kMinimumRtpHeaderLength + 4u * cc_count;

    if (header.size() < header_length_without_extn) {
      return false;
    }

    header = header.subspan(header_length_without_extn);

    uint16_t extension_count =
        base::U16FromBigEndian(header.subspan(2u).first<2u>());

    if (header.size() < (extension_count + 1u) * 4u) {
      return false;
    }

    return true;
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<base::ScopedTempDir> temp_dir_;
  base::FilePath incoming_dump_path_;
  base::FilePath outgoing_dump_path_;
  std::unique_ptr<WebRtcRtpDumpWriter> writer_;
};

TEST_F(WebRtcRtpDumpWriterTest, NoDumpFileIfNoPacketDumped) {
  // The scope is used to make sure the EXPECT_CALL is checked before exiting
  // the scope.
  {
    EXPECT_CALL(*this, OnEndDumpDone(false, false));

    writer_->EndDump(RTP_DUMP_BOTH,
                     base::BindOnce(&WebRtcRtpDumpWriterTest::OnEndDumpDone,
                                    base::Unretained(this)));

    FlushTaskRunner(writer_->background_task_runner().get());
    base::RunLoop().RunUntilIdle();
    FlushTaskRunner(writer_->background_task_runner().get());
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_FALSE(base::PathExists(incoming_dump_path_));
  EXPECT_FALSE(base::PathExists(outgoing_dump_path_));
}

TEST_F(WebRtcRtpDumpWriterTest, WriteAndFlushSmallSizeDump) {
  std::vector<uint8_t> packet_header = CreateFakeRtpPacketHeader(1u, 2u);

  writer_->WriteRtpPacket(
      &packet_header[0], packet_header.size(), 100, true);
  writer_->WriteRtpPacket(
      &packet_header[0], packet_header.size(), 100, false);

  // The scope is used to make sure the EXPECT_CALL is checked before exiting
  // the scope.
  {
    EXPECT_CALL(*this, OnEndDumpDone(true, true));

    writer_->EndDump(RTP_DUMP_BOTH,
                     base::BindOnce(&WebRtcRtpDumpWriterTest::OnEndDumpDone,
                                    base::Unretained(this)));

    FlushTaskRunner(writer_->background_task_runner().get());
    base::RunLoop().RunUntilIdle();
    FlushTaskRunner(writer_->background_task_runner().get());
    base::RunLoop().RunUntilIdle();
  }

  VerifyDumps(1, 1);
}

// Flaky test disabled on Windows (https://crbug.com/1044271).
#if BUILDFLAG(IS_WIN)
#define MAYBE_WriteOverMaxLimit DISABLED_WriteOverMaxLimit
#else
#define MAYBE_WriteOverMaxLimit WriteOverMaxLimit
#endif
TEST_F(WebRtcRtpDumpWriterTest, MAYBE_WriteOverMaxLimit) {
  // Reset the writer with a small max size limit.
  writer_ = std::make_unique<WebRtcRtpDumpWriter>(
      incoming_dump_path_, outgoing_dump_path_, 100,
      base::BindRepeating(&WebRtcRtpDumpWriterTest::OnMaxSizeReached,
                          base::Unretained(this)));

  std::vector<uint8_t> packet_header = CreateFakeRtpPacketHeader(3u, 4u);

  const size_t kPacketCount = 200;
  // The scope is used to make sure the EXPECT_CALL is checked before exiting
  // the scope.
  {
    EXPECT_CALL(*this, OnMaxSizeReached()).Times(testing::AtLeast(1));

    // Write enough packets to overflow the in-memory buffer and max limit.
    for (size_t i = 0; i < kPacketCount; ++i) {
      writer_->WriteRtpPacket(
          &packet_header[0], packet_header.size(), 100, true);

      writer_->WriteRtpPacket(
          &packet_header[0], packet_header.size(), 100, false);
    }

    EXPECT_CALL(*this, OnEndDumpDone(true, true));

    writer_->EndDump(RTP_DUMP_BOTH,
                     base::BindOnce(&WebRtcRtpDumpWriterTest::OnEndDumpDone,
                                    base::Unretained(this)));

    FlushTaskRunner(writer_->background_task_runner().get());
    base::RunLoop().RunUntilIdle();
    FlushTaskRunner(writer_->background_task_runner().get());
    base::RunLoop().RunUntilIdle();
  }
  VerifyDumps(kPacketCount, kPacketCount);
}

TEST_F(WebRtcRtpDumpWriterTest, DestroyWriterBeforeEndDumpCallback) {
  EXPECT_CALL(*this, OnEndDumpDone(testing::_, testing::_)).Times(0);

  writer_->EndDump(RTP_DUMP_BOTH,
                   base::BindOnce(&WebRtcRtpDumpWriterTest::OnEndDumpDone,
                                  base::Unretained(this)));

  writer_.reset();

  // Two |RunUntilIdle()| calls are needed as the first run posts a task that
  // we need to give a chance to run with the second call.
  base::RunLoop().RunUntilIdle();
  base::RunLoop().RunUntilIdle();
}

TEST_F(WebRtcRtpDumpWriterTest, EndDumpsSeparately) {
  std::vector<uint8_t> packet_header = CreateFakeRtpPacketHeader(1u, 2u);

  writer_->WriteRtpPacket(
      &packet_header[0], packet_header.size(), 100, true);
  writer_->WriteRtpPacket(
      &packet_header[0], packet_header.size(), 100, true);
  writer_->WriteRtpPacket(
      &packet_header[0], packet_header.size(), 100, false);

  // The scope is used to make sure the EXPECT_CALL is checked before exiting
  // the scope.
  {
    EXPECT_CALL(*this, OnEndDumpDone(true, false));
    EXPECT_CALL(*this, OnEndDumpDone(false, true));

    writer_->EndDump(RTP_DUMP_INCOMING,
                     base::BindOnce(&WebRtcRtpDumpWriterTest::OnEndDumpDone,
                                    base::Unretained(this)));

    writer_->EndDump(RTP_DUMP_OUTGOING,
                     base::BindOnce(&WebRtcRtpDumpWriterTest::OnEndDumpDone,
                                    base::Unretained(this)));

    FlushTaskRunner(writer_->background_task_runner().get());
    base::RunLoop().RunUntilIdle();
    FlushTaskRunner(writer_->background_task_runner().get());
    base::RunLoop().RunUntilIdle();
  }

  VerifyDumps(2, 1);
}
