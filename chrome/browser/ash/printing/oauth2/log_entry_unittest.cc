// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/log_entry.h"

#include <string>

#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chromeos/printing/uri.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::printing::oauth2 {
namespace {

TEST(PrintingOAuth2LogEntryTest, LogEntry) {
  const std::string log_entry = LogEntry(
      "message", "method", GURL("http://abc.de:123/fgh?ij=kl#mn"),
      StatusCode::kServerError, chromeos::Uri("http://opq.rs:456/tu?vq=x#yz"));
  EXPECT_EQ(log_entry,
            "oauth method;server=http://abc.de:123/fgh?ij=kl#mn;"
            "endpoint=http://opq.rs:456/tu?vq=x#yz;status=ServerError:"
            " message");
}

TEST(PrintingOAuth2LogEntryTest, LogEntryMessageAndMethodOnly) {
  const std::string log_entry = LogEntry("message", "method", GURL());
  EXPECT_EQ(log_entry, "oauth method: message");
}

TEST(PrintingOAuth2LogEntryTest, LogEntryEmptyMessageAndStatus) {
  const std::string log_entry =
      LogEntry("", "method", GURL("http://abc.de:123/fgh?ij=kl#mn"),
               std::nullopt, chromeos::Uri("http://opq.rs:456/tu?vq=x#yz"));
  EXPECT_EQ(log_entry,
            "oauth method;server=http://abc.de:123/fgh?ij=kl#mn;"
            "endpoint=http://opq.rs:456/tu?vq=x#yz");
}

}  // namespace
}  // namespace ash::printing::oauth2
