# Set Shape API Browser Tests (`chrome/browser/ash/set_shape`)

This directory hosts ChromeOS end-to-end browser tests for the `window.setShape`
API, which allows allowlisted unframed Isolated Web Apps (IWAs) to define custom
non-rectangular window regions and hit-test masks on ChromeOS.

## Companion Documentation

- **Blink Renderer Module:**
  [`//third_party/blink/renderer/modules/set_shape`](/third_party/blink/renderer/modules/set_shape/README.md)
- **ChromeOS Mojo Service & Allowlist Implementation:**
  [`//chromeos/ash/experiences/isolated_web_app`](/chromeos/ash/experiences/isolated_web_app/README.md)

## Test Suites

- `set_shape_browsertest.cc`: Verifies `window.setShape()` input validation
  (bounds, maximum rectangle count, minimum 10x10 visible region), unframed
  window mode requirements (`DoesNotWorkOutsideUnframedMode` and
  `ClearsShapeOnTransitionFromUnframed`), shape clearing, and window resize
  handling (`aura::Window::layer()->alpha_shape()`).
- `set_shape_allowlist_browsertest.cc`: Verifies that `'setShape' in window`
  exposure via `CrosIsolatedWebAppEnabler` and
  `ash::CanOriginAccessCrosIwaApi()` (`IwaRuntimeDataProvider` `allow_set_shape`
  permission) respects `chromeos::features::kCrosIsolatedWebAppSetShapeAllowlist`
  and `blink::features::kSetShape` across allowlisted IWAs, non-allowlisted
  IWAs, child windows, and non-IWA pages.
