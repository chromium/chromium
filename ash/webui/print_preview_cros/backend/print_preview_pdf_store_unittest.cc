// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/print_preview_cros/backend/print_preview_pdf_store.h"

#include <memory>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::printing::print_preview {

namespace {

scoped_refptr<base::RefCountedMemory> CreateTestData(
    const std::string& data = "") {
  // Mimics PDF data format + minimum length.
  return base::MakeRefCounted<base::RefCountedString>(
      "%PDF-7j4iGwz3X9bLoQkEc6u1f5t8y2hVn0pArDmJsKiLg9f4s7r3x2c1v" + data);
}

}  // namespace

class PrintPreviewPdfStoreTest : public testing::Test {
 public:
  PrintPreviewPdfStoreTest() = default;
  ~PrintPreviewPdfStoreTest() override = default;
};

// Verify requesting data for a non-existent unguessable token returns NULL
// data.
TEST_F(PrintPreviewPdfStoreTest, NonExistentToken) {
  scoped_refptr<base::RefCountedMemory> data =
      PrintPreviewPdfStore::GetInstance()->GetPdfData(
          base::UnguessableToken::Create(), /*page_index=*/0);
  EXPECT_EQ(nullptr, data);
}

// Verify requesting data for a non-existent page index returns NULL data.
TEST_F(PrintPreviewPdfStoreTest, NonExistentPageIndex) {
  const auto token = base::UnguessableToken::Create();
  const int page_index = 0;

  // Insert data for specified token and page index.
  PrintPreviewPdfStore::GetInstance()->InsertPdfData(token, page_index,
                                                     CreateTestData());

  // Try to fetch with a valid token but and invalid token and expect null.
  const int invalid_page_index = 100;
  scoped_refptr<base::RefCountedMemory> data =
      PrintPreviewPdfStore::GetInstance()->GetPdfData(token,
                                                      invalid_page_index);
  EXPECT_EQ(nullptr, data);
}

// Verify data can be stored then received.
TEST_F(PrintPreviewPdfStoreTest, GetExpectedData) {
  const auto token = base::UnguessableToken::Create();
  const int page_index = 0;
  const scoped_refptr<base::RefCountedMemory> expected_data = CreateTestData();

  PrintPreviewPdfStore::GetInstance()->InsertPdfData(token, page_index,
                                                     expected_data);

  scoped_refptr<base::RefCountedMemory> data =
      PrintPreviewPdfStore::GetInstance()->GetPdfData(token, page_index);
  EXPECT_EQ(expected_data, data);
}

// Verify data can be saved under different page indices for the same
// unguessable token.
TEST_F(PrintPreviewPdfStoreTest, SameTokenDifferentPageIndex) {
  const auto token = base::UnguessableToken::Create();
  const int first_page_index = 0;
  const scoped_refptr<base::RefCountedMemory> first_expected_data =
      CreateTestData("first");
  const int second_page_index = 1;
  const scoped_refptr<base::RefCountedMemory> second_expected_data =
      CreateTestData("second");

  PrintPreviewPdfStore::GetInstance()->InsertPdfData(token, first_page_index,
                                                     first_expected_data);
  PrintPreviewPdfStore::GetInstance()->InsertPdfData(token, second_page_index,
                                                     second_expected_data);

  scoped_refptr<base::RefCountedMemory> first_data =
      PrintPreviewPdfStore::GetInstance()->GetPdfData(token, first_page_index);
  EXPECT_EQ(first_expected_data, first_data);

  scoped_refptr<base::RefCountedMemory> second_data =
      PrintPreviewPdfStore::GetInstance()->GetPdfData(token, second_page_index);
  EXPECT_EQ(second_expected_data, second_data);
}

// Verify data can be saved under different unguessable tokens.
TEST_F(PrintPreviewPdfStoreTest, DifferentTokenSamePageIndex) {
  const auto first_token = base::UnguessableToken::Create();
  const auto second_token = base::UnguessableToken::Create();
  const int page_index = 0;
  const scoped_refptr<base::RefCountedMemory> first_expected_data =
      CreateTestData("first");
  const scoped_refptr<base::RefCountedMemory> second_expected_data =
      CreateTestData("second");

  PrintPreviewPdfStore::GetInstance()->InsertPdfData(first_token, page_index,
                                                     first_expected_data);
  PrintPreviewPdfStore::GetInstance()->InsertPdfData(second_token, page_index,
                                                     second_expected_data);

  scoped_refptr<base::RefCountedMemory> first_data =
      PrintPreviewPdfStore::GetInstance()->GetPdfData(first_token, page_index);
  EXPECT_EQ(first_expected_data, first_data);

  scoped_refptr<base::RefCountedMemory> second_data =
      PrintPreviewPdfStore::GetInstance()->GetPdfData(second_token, page_index);
  EXPECT_EQ(second_expected_data, second_data);
}

}  // namespace ash::printing::print_preview
