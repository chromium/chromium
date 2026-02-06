// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/permission_request_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "services/device/public/mojom/geoposition.mojom-forward.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-shared.h"

using ::testing::Pointee;

class GeolocationElementBrowserTest : public InProcessBrowserTest {
 public:
  GeolocationElementBrowserTest()
      : geolocation_overrider_(
            std::make_unique<device::ScopedGeolocationOverrider>(
                device::mojom::GeopositionResult::NewPosition(
                    device::mojom::Geoposition::New()),
                device::mojom::GeopositionResult::NewPosition(
                    high_accuracy_position_.Clone()))) {
    feature_list_.InitWithFeatures(
        {blink::features::kGeolocationElement,
         blink::features::kBypassPepcSecurityForTesting},
        {});
  }

  ~GeolocationElementBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        browser(), embedded_test_server()->GetURL("/empty.html"), 1));
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

 protected:
  device::mojom::GeopositionPtr high_accuracy_position_ =
      device::mojom::Geoposition::New(/*latitude=*/1.2,
                                      /*longitude=*/4.2,
                                      /*altitude=*/0.0,
                                      /*accuracy=*/10.3,
                                      /*altitude_accuracy=*/5.2,
                                      /*heading=*/0.0,
                                      /*speed=*/0.0,
                                      /*timestamp=*/base::Time::Now(),
                                      /*is_precise=*/true);
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GeolocationElementBrowserTest, GeolocationElement) {
  ASSERT_TRUE(content::ExecJs(web_contents(), R"(
    var g = document.createElement('geolocation');
    var gottenPosition = new Promise(resolve => {
      g.onlocation = function(event) {
        resolve(event.target.position.coords);
      }
    });
    document.body.appendChild(g);
  )"));

  permissions::PermissionRequestManager::FromWebContents(web_contents())
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::AutoResponseType::ACCEPT_ALL);
  permissions::PermissionRequestObserver observer(web_contents());
  ASSERT_TRUE(content::ExecJs(web_contents(), "g.click()"));
  observer.Wait();
  base::DictValue position =
      content::EvalJs(
          web_contents(),
          "(async function () { return (await gottenPosition).toJSON(); })()")
          .TakeValue()
          .TakeDict();
  EXPECT_THAT(position.Find("latitude"),
              Pointee(high_accuracy_position_->latitude));
  EXPECT_THAT(position.Find("longitude"),
              Pointee(high_accuracy_position_->longitude));
  EXPECT_THAT(position.Find("accuracy"),
              Pointee(high_accuracy_position_->accuracy));
}
