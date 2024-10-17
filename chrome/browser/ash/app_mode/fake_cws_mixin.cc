// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/fake_cws_mixin.h"

#include <cstddef>
#include <cstring>
#include <string_view>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr std::string_view kPublicStoreUpdateEndpoint = "/update_check.xml";

constexpr std::string_view kPrivateStoreUpdateEndpoint =
    "/private_store_update_check.xml";

}  // namespace

FakeCwsMixin::FakeCwsMixin(InProcessBrowserTestMixinHost* host,
                           CwsInstanceType instance_type)
    : InProcessBrowserTestMixin(host),
      instance_type_(instance_type),
      test_server_setup_mixin_(host, &test_server_) {}

FakeCwsMixin::~FakeCwsMixin() = default;

void FakeCwsMixin::SetUpCommandLine(base::CommandLine* command_line) {
  // `fake_cws_.Init` modifies the command line using static methods. It must be
  // called during SetUpCommandLine.
  instance_type_ == kPublic ? fake_cws_.Init(&test_server_)
                            : fake_cws_.InitAsPrivateStore(
                                  &test_server_, kPrivateStoreUpdateEndpoint);
}

GURL FakeCwsMixin::UpdateUrl() const {
  return test_server_.base_url().Resolve(instance_type_ == kPublic
                                             ? kPublicStoreUpdateEndpoint
                                             : kPrivateStoreUpdateEndpoint);
}

}  // namespace ash
