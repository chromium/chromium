# OOBE Test API

This directory contains is a API for automating OOBE and login process in
various tests.

The following tests use this API:
* Catapult telemetry tests, located in Chromium under `third_party/catapult`,
  mostly `third_party/catapult/telemetry/telemetry/internal/backends/chrome/oobe.py`.
* Autotests, located in ChromiumOS under `src/third_party/autotests`, mostly
  `src/third_party/autotest/files/client/common_lib/cros/enrollment.py` and
  files under `src/third_party/autotest/files/client/site_tests/`.
* Tast tests, located in ChromiumOS under `src/platform/tast-tests/`, mostly
  under `src/platform/tast-tests/src/chromiumos/tast/local/bundles/cros/`.
