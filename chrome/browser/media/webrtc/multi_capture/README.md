# Multi-Capture Browser Services (`chrome/browser/media/webrtc/multi_capture`)

This directory implements ChromeOS browser-side policy verification, session
tracking, and privacy indicators for the `getAllScreensMedia` WebRTC API, which
is restricted to enterprise-allowlisted Isolated Web Apps (IWAs).

## Companion Documentation

- **Core IWA Component:**
  [`//components/webapps/isolated_web_apps`](/components/webapps/isolated_web_apps/README.md)
- **Browser IWA Engine:**
  [`//chrome/browser/web_applications/isolated_web_apps`](/chrome/browser/web_applications/isolated_web_apps/README.md)

## Key Components

- `MultiCaptureDataService` (`multi_capture_data_service.{h,cc}` &
  `multi_capture_data_service_factory.{h,cc}`): Evaluates the login-time
  `MultiScreenCaptureAllowedForUrls` policy, tracks installed allowlisted IWAs
  and their icons via `WebAppInstallManagerObserver`, and consults
  `IwaRuntimeDataProvider` (`GetSkipMultiCaptureNotificationBundleIds()`) to
  determine which apps require capture notifications.
- `MultiCaptureSessionController` (`multi_capture_session_controller.{h,cc}` &
  `multi_capture_session_controller_factory.{h,cc}`): Observes
  `user_manager::UserManager` and `session_manager::SessionManager` to
  automatically terminate active multi-screen capture streams
  (`StopAllCaptures()`) when the active user switches or the ChromeOS session
  state changes.
- `MultiCaptureUsageIndicatorService`
  (`multi_capture_usage_indicator_service.{h,cc}` &
  `multi_capture_usage_indicator_service_factory.{h,cc}`): Displays persistent
  ChromeOS system notifications and privacy indicators both before login/install
  (reporting which allowlisted apps may capture the screen in the future) and
  while active multi-screen capture sessions are running.
