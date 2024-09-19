// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_FAKE_CWS_H_
#define CHROME_BROWSER_ASH_APP_MODE_FAKE_CWS_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/callback_forward.h"
#include "extensions/browser/scoped_ignore_content_verifier_for_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace ash {

// Simple fake CWS update check request handler that returns a fixed update
// check response. The response is created either from SetUpdateCrx() or
// SetNoUpdate().
class FakeCWS {
 public:
  FakeCWS();
  FakeCWS(const FakeCWS&) = delete;
  FakeCWS& operator=(const FakeCWS&) = delete;
  ~FakeCWS();

  // Initializes as CWS request handler and overrides app gallery command line
  // switches.
  void Init(net::EmbeddedTestServer* embedded_test_server);

  // Initializes as a private store handler using the given server and URL end
  // point. Override app gallery command line and provide it to Extensions
  // client.
  void InitAsPrivateStore(net::EmbeddedTestServer* embedded_test_server,
                          std::string_view update_check_end_point);

  // Sets up the update check response with has_update template.
  void SetUpdateCrx(std::string_view app_id,
                    std::string_view crx_file,
                    std::string_view version);

  // Sets up the update check response with no_update template.
  void SetNoUpdate(std::string_view app_id);

  // Set the details to be returned via Chrome Web Store details query.
  void SetAppDetails(std::string_view app_id,
                     std::string localized_name,
                     std::string icon_url,
                     std::string manifest_json);

  // Returns the current `update_check_count_` and resets it.
  int GetUpdateCheckCountAndReset();

 private:
  enum class GalleryUpdateMode {
    kOnlyCommandLine,
    kModifyExtensionsClient,
  };

  struct AppDetails {
    std::string localized_name;
    std::string icon_url;
    std::string manifest_json;
  };

  void SetupWebStoreURL(const GURL& test_server_url);
  void OverrideGalleryCommandlineSwitches();

  bool GetUpdateCheckContent(const std::vector<std::string>& ids,
                             std::string* update_check_content,
                             bool use_json);

  // Creates serialized protobuf string of an item snippet API response. Returns
  // nullopt if the `app_id` is not in `id_to_details_map_`.
  std::optional<std::string> CreateItemSnippetStringForApp(
      const std::string& app_id);

  // Request handler for kiosk app update server.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

  GURL web_store_url_;

  // Used to override the item snippets API URL for the embedded test server.
  GURL item_snippets_url_;
  std::optional<base::AutoReset<const GURL*>> item_snippets_url_override_;

  bool use_private_store_templates_;
  std::string update_check_end_point_;

  // Map keyed by app_id to partially-bound functions that can generate the
  // app's update content.
  std::map<std::string, base::RepeatingCallback<std::string(bool, bool)>>
      id_to_update_check_content_map_;
  int update_check_count_;

  // Map keyed by app_id to app details. These are details returned via a
  // special request to Chrome Web Store and normally used to render app's item
  // in the kiosk app menu. Since test which use them don't rely on these
  // details so far, only two necessary ones are supported at the moment (see
  // the AppDetails struct).
  std::map<std::string, AppDetails> id_to_details_map_;

  // FakeCWS overrides Chrome Web Store URLs, so extensions it provides in tests
  // are considered as extensions from Chrome Web Store. ContentVerifier assumes
  // that Chrome Web Store provides signed hashes for its extensions' resources
  // (a.k.a. verified_contents.json). FakeCWS currently doesn't provide such
  // hashes, so content verification fails with MISSING_ALL_HASHES, which is
  // considered as a corruption. In order to not false-positively detect
  // corruption in test extensions, disable content verification.
  // TODO(https://crbug.com/1051560) Make FakeCWS support this feature of the
  // real Chrome Web Store and remove this scoped ignore.
  std::unique_ptr<extensions::ScopedIgnoreContentVerifierForTest>
      scoped_ignore_content_verifier_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_FAKE_CWS_H_
