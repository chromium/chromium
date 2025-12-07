// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/fake_cws_mixin.h"

#include <string_view>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "extensions/common/verifier_formats.h"
#include "net/test/embedded_test_server/http_request.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr std::string_view kPublicStoreUpdateEndpoint = "/update_check.xml";

constexpr std::string_view kPrivateStoreUpdateEndpoint =
    "/private_store_update_check.xml";

void LogRequest(const net::test_server::HttpRequest& request) {
  LOG(INFO) << "Received " << request.method_string << " request for url '"
            << request.GetURL() << "'";
}

}  // namespace

FakeCwsMixin::FakeCwsMixin(InProcessBrowserTestMixinHost* host,
                           CwsInstanceType instance_type)
    : InProcessBrowserTestMixin(host),
      instance_type_(instance_type),
      fake_origin_mixin_(
          host,
          /*origin=*/
          GURL(instance_type == kPublic ? "https://fakecws.com/"
                                        : "https://selfhostedappserver.com/"),
          /*path_to_be_served=*/FILE_PATH_LITERAL("chrome/test/data")) {
  if (instance_type == kPublic) {
    // When installing or updating a CRX, the extension code may verify the CRX
    // is signed by the real CWS via verified_contents.json. It is difficult to
    // create a valid CRX and verified_contents that passes that check, so we
    // disable this verification in tests using `FakeCWS` in `kPublic` mode.
    disable_crx_publisher_verification_ =
        extensions::DisablePublisherKeyVerificationForTests();
  }
}

FakeCwsMixin::~FakeCwsMixin() = default;

void FakeCwsMixin::SetUpCommandLine(base::CommandLine* command_line) {
  // `fake_cws_.Init` modifies the command line using static methods. It must be
  // called during SetUpCommandLine.
  instance_type_ == kPublic
      ? fake_cws_.Init(&fake_origin_mixin_.server())
      : fake_cws_.InitAsPrivateStore(&fake_origin_mixin_.server(),
                                     fake_origin_mixin_.origin(),
                                     kPrivateStoreUpdateEndpoint);
}

void FakeCwsMixin::SetUpOnMainThread() {
  fake_origin_mixin_.server().RegisterRequestMonitor(
      base::BindRepeating(&LogRequest));
}

GURL FakeCwsMixin::UpdateUrl() const {
  return fake_origin_mixin_.GetUrl(instance_type_ == kPublic
                                       ? kPublicStoreUpdateEndpoint
                                       : kPrivateStoreUpdateEndpoint);
}

}  // namespace ash
