// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_diagnostics_builder.h"

#include "chrome/browser/ash/guest_os/guest_os_diagnostics.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace guest_os {

namespace {
using Status = mojom::DiagnosticEntry::Status;
}  // namespace

class GuestOsDiagnosticsBuilderTest : public testing::Test {
 public:
  using EntryBuilder = DiagnosticsBuilder::EntryBuilder;
};

void CheckMessage(const mojom::DiagnosticMessagePtr& message_with_link,
                  const std::string& message,
                  const GURL& link) {
  ASSERT_FALSE(message_with_link.is_null());
  EXPECT_EQ(message_with_link->message, message);
  EXPECT_EQ(message_with_link->learn_more_link, link);
}

void CheckDiagnosticEntry(const mojom::DiagnosticEntryPtr& entry,
                          const std::string& requirement,
                          Status status,
                          const std::string& explanation,
                          const GURL& learn_more_link) {
  ASSERT_FALSE(entry.is_null());
  EXPECT_EQ(entry->requirement, requirement);
  EXPECT_EQ(entry->status, status);

  CheckMessage(entry->explanation, explanation, learn_more_link);
}

// Test that top error is set iff it has not been set already.
TEST_F(GuestOsDiagnosticsBuilderTest, TopError) {
  DiagnosticsBuilder builder;
  {
    EntryBuilder entry("foo");
    entry.SetFail("foo is wrong", GURL("http://foo-is-wrong"));
    builder.AddEntry(std::move(entry));
  }
  {
    EntryBuilder entry("bar");
    entry.SetFail("bar is wrong", GURL("http://bar-is-wrong"));
    builder.AddEntry(std::move(entry));
  }
  auto diagnostics = builder.Build();
  auto& entries = diagnostics->entries;
  ASSERT_EQ(entries.size(), 2u);

  CheckDiagnosticEntry(entries[0], "foo", Status::kFail, "foo is wrong",
                       GURL("http://foo-is-wrong"));
  CheckDiagnosticEntry(entries[1], "bar", Status::kFail, "bar is wrong",
                       GURL("http://bar-is-wrong"));

  CheckMessage(diagnostics->top_error, "foo is wrong",
               GURL("http://foo-is-wrong"));
}

// Test override top error.
TEST_F(GuestOsDiagnosticsBuilderTest, OverrideTopError) {
  DiagnosticsBuilder builder;
  {
    EntryBuilder entry("foo");
    entry.SetFail("foo is wrong", GURL("http://foo-is-wrong"))
        .OverrideTopError("foo is so wrong", GURL("http://foo-is-so-wrong"));
    builder.AddEntry(std::move(entry));
  }
  auto diagnostics = builder.Build();
  auto& entries = diagnostics->entries;
  ASSERT_EQ(entries.size(), 1u);

  CheckDiagnosticEntry(entries[0], "foo", Status::kFail, "foo is wrong",
                       GURL("http://foo-is-wrong"));

  CheckMessage(diagnostics->top_error, "foo is so wrong",
               GURL("http://foo-is-so-wrong"));
}

}  // namespace guest_os
