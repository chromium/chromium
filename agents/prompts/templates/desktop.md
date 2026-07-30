# Desktop Chrome Instructions

Instructions that are relevant when targeting desktop platforms (when
`{OUT_DIR}/args.gn` contains `target_os="linux"`, `target_os="mac"`,
`target_os="windows"`, or when `target_os` is not set).

## Context

Before starting any tasks, you **MUST** read the following files to better
understand design principles and commonly components within Chrome.
  *  `//docs/chrome_browser_design_principles.md`
  *  `//docs/ui/views/overview.md`

## Build Targets
Always build relevant targets after making edits. Typical targets could be:
  * `chrome` - the main binary for desktop chrome
  * `unit_tests` - unit-style tests for desktop chrome
  * `browser_tests` - integration test for desktop chrome
  * `interactive_ui_tests` - integration tests for desktop chrome that
    cannot be run in parallel as they require exclusive control of OS or
    window-manager.
  * `blink_tests` - target containing `content_shell` and dependencies needed
    to run Blink web tests (`run_web_tests.py`).

## Running Tests on Linux

When running test targets on Linux, execute them under a virtual X server
(`Xvfb`) to avoid display dependency errors and prevent test windows from
stealing focus on the desktop.

* **C++ Test Binaries (`unit_tests`, `browser_tests`, `interactive_ui_tests`):**
  Prefix invocations with `testing/xvfb.py` as described in
  `//docs/linux/debugging.md`:
  ```bash
  testing/xvfb.py out/Default/browser_tests --gtest_filter=TestName.*
  ```

* **Blink Web Tests (`blink_tests` / `run_web_tests.py`):**
  When running Blink web tests on Linux as described in
  `//docs/testing/web_tests_linux.md`, run under an Xvfb display:
  ```bash
  Xvfb :4 -screen 0 1024x768x24 &
  DISPLAY=:4 run_web_tests.py
  ```
