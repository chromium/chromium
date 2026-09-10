# Browser Actuator Internals

This directory contains internal components and diagnostics for Browser Actuator.

## `chrome://browser-actuator-internals` WebUI

Backend and WebUIController (`browser_actuator_internals_ui.*`, `browser_actuator_internals_ui_mojo_impl.*`) for `chrome://browser-actuator-internals`.

### Access & Verification

To access and manually verify the page:

1. Launch Chrome with the required feature flags:
   ```bash
   --enable-features=BrowserActuator,BrowserActuatorInternals
   ```
2. Navigate to `chrome://browser-actuator-internals/`.

No external hardware or additional user actions are required; navigating to the page immediately renders the diagnostic UI.

### Testing

```bash
# Browser tests (verifies page load, LitElement DOM, and Mojo connection)
out/Default/browser_tests --gtest_filter="BrowserActuatorInternalsUIBrowserTest.*"

# Backend unit tests (verifies Mojo interface implementation)
out/Default/unit_tests --gtest_filter="BrowserActuatorInternalsUIMojoImplTest.*"
```

