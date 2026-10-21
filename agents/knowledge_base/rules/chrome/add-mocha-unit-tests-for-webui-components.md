---
name: chrome/add-mocha-unit-tests-for-webui-components
rubric: testing
severity: should_fix
---

**Rule.** Add dedicated WebUI Mocha unit tests under chrome/test/data/webui/
executed via WebUIMochaBrowserTest for new or modified WebUI custom elements and
components. Do not rely solely on C++ interactive UI tests, browser tests, or
pixel tests as a substitute for isolated component unit tests.

**Why.** Mocha unit tests execute fast, isolate component loading and user
interactions without full browser overhead, and prevent regressions in UI
element state transitions, event dispatching, and DOM binding.

## Examples

Bad:

```cpp
// Adding or modifying a WebUI component (e.g. report_unsafe_site_app.ts)
// without adding Mocha unit tests under chrome/test/data/webui/,
// relying solely on planned C++ interactive UI tests or pixel tests.
```

Good:

```cpp
// chrome/test/data/webui/feedback/report_unsafe_site_test.ts
import 'chrome://feedback/report_unsafe_site_app.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('ReportUnsafeSiteTest', () => {
  test('LoadWithoutErrors', () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const app = document.createElement('report-unsafe-site-app');
    document.body.appendChild(app);
    const cancelButton =
        app.shadowRoot.querySelector<HTMLInputElement>('.cancel-button');
    assertTrue(!!cancelButton);
  });
});
```
