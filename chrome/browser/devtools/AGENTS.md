# Chrome DevTools (`chrome/browser/devtools`)

This directory contains Chrome-layer DevTools integration (such as DevTools window management, UI bindings, and remote debugging) as well as Chrome-specific Chrome DevTools Protocol (CDP) domain handlers in `chrome/browser/devtools/protocol/` (e.g., `Autofill`, `Browser`, `Extensions`, `PWA`, `Target`, and Chrome-specific `Page` commands).

## CDP Testing Guidelines

For general CDP testing principles, see the canonical guide:
- **[`/content/browser/devtools/protocol/AGENTS.md`](/content/browser/devtools/protocol/AGENTS.md)**

When adding or modifying CDP functionality in `chrome/browser/devtools/protocol/`:

- **Do NOT add new protocol tests to `devtools_protocol_browsertest.cc`:** Avoid adding new C++ tests to `chrome/browser/devtools/protocol/devtools_protocol_browsertest.cc` when the behavior can be tested via JavaScript protocol tests.
- **Features testable in `content_shell`:** If the protocol command or event is available in `content_shell`, author a JavaScript Inspector Protocol test in `third_party/blink/web_tests/inspector-protocol/` (see [`/third_party/blink/web_tests/inspector-protocol/AGENTS.md`](/third_party/blink/web_tests/inspector-protocol/AGENTS.md)).
- **Chrome-layer protocol features (`HeadlessModeProtocolBrowserTest`):** Because `content_shell` does not include `chrome/` layer handlers, Chrome-specific protocol domains (such as `Autofill`, `Browser`, `Extensions`, or window management) should be tested using JavaScript protocol tests run via `HeadlessModeProtocolBrowserTest`:
  - Place test scripts and `-expected.txt` files in `chrome/browser/headless/test/data/protocol/<domain>/` (or `components/headless/test/data/protocol/`).
  - Register the test in `chrome/browser/headless/test/headless_mode_protocol_browsertest.cc` using `HEADLESS_MODE_PROTOCOL_TEST(TestName, "<domain>/<script-name>.js")`.
  - Run with `browser_tests --gtest_filter="HeadlessModeProtocolBrowserTest.<TestName>"` (pass `--reset-results` to update expectation files).
- **C++ Tests (Last Resort Only):** Use C++ unit tests (`*_unittest.cc`) or C++ browser tests only as a last resort to exercise low-level logic or Chrome internals that cannot be fully covered by a JavaScript protocol test.

## Related Documentation
- [Canonical CDP Testing Guide (`/content/browser/devtools/protocol/AGENTS.md`)](/content/browser/devtools/protocol/AGENTS.md)
- [Authoring Inspector Protocol Tests (`/third_party/blink/web_tests/inspector-protocol/AGENTS.md`)](/third_party/blink/web_tests/inspector-protocol/AGENTS.md)
- [README.md](README.md)
