// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/access_code_cast/access_code_cast_integration_browsertest.h"

#include "chrome/browser/media/router/discovery/access_code/access_code_cast_constants.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_media_sink_util.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_test_util.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
namespace media_router {

namespace {
const char kEndpointResponseSuccess[] =
    R"({
      "device": {
        "displayName": "test_device",
        "id": "1234",
        "deviceCapabilities": {
          "videoOut": true,
          "videoIn": true,
          "audioOut": true,
          "audioIn": true,
          "devMode": true
        },
        "networkInfo": {
          "hostName": "GoogleNet",
          "port": "666",
          "ipV4Address": "192.0.2.146",
          "ipV6Address": "2001:0db8:85a3:0000:0000:8a2e:0370:7334"
        }
      }
    })";
}  // namespace

class AccessCodeCastSinkServiceBrowserTest
    : public AccessCodeCastIntegrationBrowserTest {};

IN_PROC_BROWSER_TEST_F(AccessCodeCastSinkServiceBrowserTest,
                       PRE_InstantExpiration) {
  // This pre test adds a device successfully to the browser. The next test
  // then ensures the devices was not saved when the browsertest starts up
  // again.

  // Mock a successful fetch from our server.
  SetEndpointFetcherMockResponse(kEndpointResponseSuccess, net::HTTP_OK,
                                 net::OK);

  EnableAccessCodeCasting();

  SetUpPrimaryAccountWithHostedDomain(signin::ConsentLevel::kSync,
                                      browser()->profile());

  auto* dialog_contents = ShowDialog();
  SetAccessCode("abcdef", dialog_contents);
  ExpectStartRouteCallFromTabMirroring(
      "cast:<1234>",
      MediaSource::ForTab(
          sessions::SessionTabHelper::IdForTab(web_contents()).id())
          .id(),
      web_contents());

  PressSubmitAndWaitForClose(dialog_contents);

  // Simulate the route opening and then ending. The device should expire
  // once the route ends.
  MediaRoute media_route_cast = CreateRouteForTesting("cast:<1234>");
  UpdateRoutes({media_route_cast});
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*mock_cast_media_sink_service_impl(), DisconnectAndRemoveSink(_));
  UpdateRoutes({});
  WaitForPrefRemoval("cast:<1234>");
  base::RunLoop().RunUntilIdle();

  // Now we have to wait for the call to disconnect and remove the sink.
  SpinRunLoop(AccessCodeCastSinkService::kExpirationDelay +
              base::Milliseconds(200));
  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);

  // The device should not be stored in the pref service and not in the media
  // router.
  EXPECT_FALSE(
      GetPrefUpdater()->GetMediaSinkInternalValueBySinkId("cast:<1234>"));
}

IN_PROC_BROWSER_TEST_F(AccessCodeCastSinkServiceBrowserTest,
                       InstantExpiration) {
  // This test is run after an instant expiration device was successfully
  // added to the browser. Upon restart it should not exists in prefs nor should
  // it be added to the media router.
  EXPECT_FALSE(
      GetPrefUpdater()->GetMediaSinkInternalValueBySinkId("cast:<1234>"));

  mock_cast_media_sink_service_impl()
      ->task_runner()
      ->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&CastMediaSinkServiceImpl::HasSink,
                         base::Unretained(mock_cast_media_sink_service_impl()),
                         "cast:<1234>"),
          base::BindOnce(&AccessCodeCastIntegrationBrowserTest::
                             ExpectMediaRouterHasNoSinks,
                         weak_ptr_factory_.GetWeakPtr()));
}

IN_PROC_BROWSER_TEST_F(AccessCodeCastSinkServiceBrowserTest, PRE_SavedDevice) {
  // This pre test adds a device successfully to the browser. The next test then
  // ensures the devices was saved when the browsertest starts up again.

  // Mock a successful fetch from our server.
  SetEndpointFetcherMockResponse(kEndpointResponseSuccess, net::HTTP_OK,
                                 net::OK);

  EnableAccessCodeCasting();

  SetUpPrimaryAccountWithHostedDomain(signin::ConsentLevel::kSync,
                                      browser()->profile());

  // Set the saved devices pref value.
  browser()->profile()->GetPrefs()->Set(
      prefs::kAccessCodeCastDeviceDuration,
      base::Value(static_cast<int>(base::Hours(10).InSeconds())));

  auto* dialog_contents = ShowDialog();
  SetAccessCode("abcdef", dialog_contents);
  ExpectStartRouteCallFromTabMirroring(
      "cast:<1234>",
      MediaSource::ForTab(
          sessions::SessionTabHelper::IdForTab(web_contents()).id())
          .id(),
      web_contents());

  PressSubmitAndWaitForClose(dialog_contents);

  // Simulate the route opening and then ending. The device should NOT expire
  // once the route ends.
  MediaRoute media_route_cast = CreateRouteForTesting("cast:<1234>");
  UpdateRoutes({media_route_cast});
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*mock_cast_media_sink_service_impl(), DisconnectAndRemoveSink(_))
      .Times(0);
  UpdateRoutes({});
  base::RunLoop().RunUntilIdle();

  // Now we have to wait for the call to disconnect and remove the sink (it
  // doesn't happen in this case but we must prove for correctness).
  SpinRunLoop(AccessCodeCastSinkService::kExpirationDelay +
              base::Milliseconds(200));
  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);

  // The device should be stored in the pref service and still in the media
  // router.
  EXPECT_TRUE(
      GetPrefUpdater()->GetMediaSinkInternalValueBySinkId("cast:<1234>"));
}

IN_PROC_BROWSER_TEST_F(AccessCodeCastSinkServiceBrowserTest, SavedDevice) {
  // This test is run after a saved device was successfully added to the
  // browser. Upon restart it should exists in prefs && it should be added
  // to the media router.
  AddScreenplayTag(AccessCodeCastIntegrationBrowserTest::
                       kAccessCodeCastSavedDeviceScreenplayTag);

  EXPECT_TRUE(
      GetPrefUpdater()->GetMediaSinkInternalValueBySinkId("cast:<1234>"));

  mock_cast_media_sink_service_impl()
      ->task_runner()
      ->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&CastMediaSinkServiceImpl::HasSink,
                         base::Unretained(mock_cast_media_sink_service_impl()),
                         "cast:<1234>"),
          base::BindOnce(
              &AccessCodeCastIntegrationBrowserTest::ExpectMediaRouterHasSink,
              weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace media_router
