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
