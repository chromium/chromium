# ![Logo](public_transit.webp) Debugging Public Transit Tests

Built-in debugging utilities and transition timeout notes. For core concepts,
see the [README](README.md). For authoring and review guidance, see
[Best Practices](best_practices.md).

[TOC]

## ViewPrinter {#viewprinter}

[`ViewPrinter`] prints a compact View hierarchy to logcat, useful both when
writing `ViewElement`s and when inspecting what was on screen on a failure:

```java
ViewPrinter.printActivityDecorView(page.getActivity());
```

Example output with default options:

```
@id/control_container | ToolbarControlContainer
├── @id/toolbar_container | ToolbarViewResourceFrameLayout
│   ╰── @id/toolbar | ToolbarPhone
│       ├── @id/home_button | HomeButton
│       ├── @id/location_bar | LocationBarPhone
│       │   ├── @id/location_bar_status | StatusView
│       │   │   ╰── @id/location_bar_status_icon_view | StatusIconView
│       │   │       ╰── @id/location_bar_status_icon_frame | FrameLayout
│       │   │           ╰── @id/loc_bar_status_icon | ChromeImageView
│       │   ╰── "about:blank" | @id/url_bar | UrlBarApi26
│       ╰── @id/toolbar_buttons | LinearLayout
│           ├── @id/tab_switcher_button | ToggleTabStackButton
│           ╰── @id/menu_button_wrapper | MenuButton
│               ╰── @id/menu_button | ChromeImageButton
╰── @id/tab_switcher_toolbar | StartSurfaceToolbarView
    ├── @id/new_tab_view | LinearLayout
    │   ├── AppCompatImageView
    │   ╰── "New tab" | MaterialTextView
    ╰── @id/menu_anchor | FrameLayout
        ╰── @id/menu_button_wrapper | MenuButton
            ╰── @id/menu_button | ChromeImageButton
```

[`ViewPrinter`]: https://source.chromium.org/search?q=symbol:org.chromium.base.test.util.ViewPrinter&ss=chromium

## Debugging Options {#debugging-options}

[`PublicTransitConfig`] adjusts runtime transition behavior for interactive
debugging:

* `setTransitionPauseForDebugging()` causes the test to run more slowly, pausing
  for some time after each transition and displaying a Toast with which Station
  is active (`1500 ms` is a good default).
* `setOnExceptionCallback()` runs the given callback when an exception happens
  during a transition, before the app is closed.
* `setFreezeOnException()` freezes the test on failure so you can inspect the
  live screen before teardown closes the instrumented app.

[`PublicTransitConfig`]: https://source.chromium.org/search?q=symbol:PublicTransitConfig%20f:base%2Ftest&ss=chromium

## Timeouts and Failure Reports {#timeouts}

Transition timeouts share a single polling window computed by
`ConditionWaiter.calculateTimeoutMs()` (defaults: **`5000 ms`**
`ConditionWaiter.MAX_TIME_TO_POLL`, **`10000 ms`**
`ActivityElement.NEW_TASK_TIMEOUT_MS` for `inNewTask()`, and **`12000 ms`**
`EntryPointSentinelStation.ENTRY_POINT_TIMEOUT_MS` for entry points).

* **Check `[OK  ]` timestamps before blaming `[FAIL]`**: every `Condition`'s
  timer starts when the transition starts, not when upstream suppliers fulfill.
  A slow precondition eats the shared budget (`{fulfilled after X~Y ms}`),
  leaving little time for downstream `Condition`s.
* **Trace `[BLKD]` lines upstream**: blocked `Condition`s short-circuited on an
  unready `dependOnSupplier()` input. Follow `blocked on [N]` back to the actual
  `[FAIL]` root cause.
* **Prefer `Condition#withTimeout()` over `TripBuilder#withTimeout()`**: setting
  a timeout on the genuinely slow `Condition` extends `ConditionWaiter`'s window
  only for transitions that include it. Verify from timestamps that the duration
  is expected before raising a timeout.
* **`AppNotIdleException` during clicks**: `clickTo()` / `longPressTo()` use
  [`ForgivingClickAction`], whose Javadoc documents when Espresso's main-looper
  idle wait is forgiven after the tap's `UP` event.

[`ForgivingClickAction`]: https://source.chromium.org/search?q=symbol:org.chromium.base.test.util.ForgivingClickAction&ss=chromium
