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

### Using the page

- **Flags**: Launch Chrome with
  `--enable-features=BrowserActuator,EnableBrowserActuatorForGlicExperimentalTriggering,BrowserActuatorInternals`.
- Open `chrome://browser-actuator-internals`. Reload to see new data.
- Recording starts at browser startup only if
  `BrowserActuatorInternals` is on.
- Not available in Incognito / off-the-record profiles.
- Payload bodies show as base64 for now.

When loaded, `chrome://browser-actuator-internals` queries the
backend via Mojo (`BrowserActuatorInternalsUI.GetSessionHistory`)
for all recorded transport sessions:
- Each session is displayed as a card showing its `session_id`,
  status (`Active` or `Closed`), start and end wall times, lifetime
  downstream/upstream message counts, and total events.
- Recorded events are wrapped in a collapsible details element
  ("Events (N)" or "Events (showing N of M)"), closed by default.
  Each event row displays its timestamp, direction ("Downstream" or
  "Upstream"), payload types, and message text.
- Messages longer than 80 characters are collapsible with an
  80-character preview, expandable to inspect full message text.
- If a message was truncated by the backend to fit the size
  limit, a `(truncated at 8 KB)` marker appears beside the message
  text, with an explanatory note in the expanded message details.
- If no sessions have been recorded yet, an empty-state message
  indicates no sessions yet.

### Testing

```bash
# Browser tests (verifies page load, LitElement DOM, and Mojo connection)
out/Default/browser_tests --gtest_filter="BrowserActuatorInternalsUIBrowserTest.*"
out/Default/browser_tests --gtest_filter="BrowserActuatorInternalsAppBrowserTest.*"

# Backend unit tests (verifies Mojo interface implementation)
out/Default/unit_tests --gtest_filter="BrowserActuatorInternalsUIMojoImplTest.*"
```

