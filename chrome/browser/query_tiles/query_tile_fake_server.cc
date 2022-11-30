// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/android/test_support_jni_headers/QueryTileFakeServer_jni.h"
#include "chrome/browser/android/profile_key_util.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/query_tiles/tile_service_factory.h"
#include "components/query_tiles/switches.h"
#include "components/query_tiles/test/fake_server_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using net::test_server::EmbeddedTestServer;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace {

// An instance of test server responding with the required fake tiles.
static std::unique_ptr<EmbeddedTestServer> s_embedded_test_server;

std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    int levels,
    int tiles_per_level,
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  std::string proto =
      query_tiles::FakeServerResponse::CreateServerResponseProto(
          levels, tiles_per_level);
  response->set_content(proto);
  return std::move(response);
}

void OnTilesFetched(const ScopedJavaGlobalRef<jobject>& j_callback,
                    bool success) {
  base::android::RunBooleanCallbackAndroid(j_callback, success);
}

}  // namespace

JNI_EXPORT void JNI_QueryTileFakeServer_SetupFakeServer(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_callback,
    jint levels,
    jint tiles_per_level) {
  s_embedded_test_server = std::make_unique<EmbeddedTestServer>();
  s_embedded_test_server->RegisterRequestHandler(
      base::BindRepeating(&HandleRequest, levels, tiles_per_level));
  CHECK(s_embedded_test_server->Start());
  GURL url = s_embedded_test_server->GetURL("/fake_server_url");
  query_tiles::FakeServerResponse::SetTileFetcherServerURL(url);

  auto* profile_key = android::GetLastUsedRegularProfileKey();
  query_tiles::TileService* tile_service =
      query_tiles::TileServiceFactory::GetInstance()->GetForKey(profile_key);
  tile_service->StartFetchForTiles(
      false, base::BindOnce(&OnTilesFetched,
                            ScopedJavaGlobalRef<jobject>(j_callback)));
}
