---
name: android-do-not-mock
description: >-
  How to write (or fix) Chromium Java unit tests (Robolectric / JUnit) without
  mocking or spying android.view.View, android.app.Activity, or their
  subclasses. Use when removing @SuppressWarnings("DoNotMock"), fixing a
  "DoNotMock" Error Prone error, replacing Mockito.mock(View.class) /
  @Mock Activity / spy(view), or when a test introduces a custom TestView /
  TestRecyclerView / TestActivity subclass.
---

# Writing Tests Without Mocking View / Activity

Chromium's custom Error Prone `DoNotMock` check rejects mocks **and spies** of
`android.view.View`, `android.app.Activity` and their subclasses (RecyclerView,
FrameLayout, ListView, ImageButton, ComponentActivity, ...). Mocked framework
classes return defaults for everything (0, null, false), silently diverge from
real behaviour, and make tests assert on implementation details. The check lives
in
`tools/android/errorprone_plugin/src/org/chromium/tools/errorprone/plugin/ChromiumDoNotMockChecker.java`.

Mocking *other* types is fine: `Window`, `ViewTreeObserver`,
`WindowInsetsController`, `RecyclerView.Adapter`, `LayoutManager`,
`InputMethodManager`, `AppTask`, `MenuItem`, `DragEvent`, delegates, etc.

## Decision order (prefer earlier options)

1. **Real object, real state.** `new View(context)`, `new FrameLayout(...)`,
   `Robolectric.buildActivity(Activity.class).setup().get()`. Assert on
   observable state, not on calls.
2. **Built-in Robolectric shadow.** `Shadows.shadowOf(x)` exposes recorded state
   for most things you'd otherwise `verify()`.
3. **Re-judge the assert.** If the only reason for a subclass is to count or
   capture an internal framework call, ask whether the assert is meaningful to
   what the test verifies. Replace it with a behavioural check, or drop it if
   another assert already covers the behaviour.
4. **Fake a non-View collaborator.** Subclass/spy/mock a `LayoutManager`,
   `Adapter`, `PackageManager` (via shadow), `Window`, `ViewTreeObserver`.
5. **Minimal custom subclass (last resort).** Only when production calls a
   method whose result must be controlled and Robolectric can't set it. Keep it
   to a single override, `private static`, and comment *why* it's needed.

Never re-introduce `@SuppressWarnings("DoNotMock")` as the fix.

## Getting a real Context / Activity

```java
Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
// or: ActivityController<Activity> c = Robolectric.buildActivity(Activity.class);
//     c.create();  ... c.start().resume();  ... c.pause().stop();
Context context = ApplicationProvider.getApplicationContext();
```

- Use `setup()` (create/start/resume/visible) when views need to be shown,
  focusable, or attached; `create()` is not enough for focus / `isShown()`.
- Pass an intent instead of overriding `getIntent()`:
  `Robolectric.buildActivity(Activity.class, intent)`.
- Lifecycle state: drive the real `ActivityController` instead of mocking
  `getLifecycle()`.
- `isChangingConfigurations() == true`: `controller.recreate()` sets it on the
  original instance (assert it as a precondition).
- Themed resources: `activity.setTheme(R.style.Theme_BrowserUI_DayNight)`.
- Prefer `Robolectric.buildActivity(MyActivity.class)` over `new MyActivity()`:
  `new` leaves the base Context null, so any un-overridden Context method NPEs.
  Robolectric can instantiate `private static` nested Activity classes.

## Recipes: replacing common mock/verify patterns

### Geometry, layout, location

| Instead of mocking                        | Do                                                                                                                                                                                                                                                                    |
| ----------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `getWidth/getHeight/getTop/...`           | `view.layout(l, t, r, b)` (or `measure()` + `layout()`), or `setLeft/setTop/...`                                                                                                                                                                                      |
| `getMeasuredWidth/Height`                 | real `measure(spec, spec)`; size content with `setMinimumWidth/Height` or LayoutParams                                                                                                                                                                                |
| `getLocationInWindow/OnScreen`            | attach the view: put it in a laid-out parent that is the Activity content view, or `context.getSystemService(WindowManager.class).addView(root, lp)` + `ShadowLooper.idleMainLooper()`, then position via margins/`layout()`. **Detached views always report (0,0).** |
| `getGlobalVisibleRect`                    | real laid-out tree (e.g. FrameLayout root at 1000x2000 with gravity)                                                                                                                                                                                                  |
| `transformMatrixToGlobal` / rotation      | `parent.setRotation(30f)` etc.                                                                                                                                                                                                                                        |
| `getWindowVisibleDisplayFrame` (detached) | returns display size; set `@Config(qualifiers = "h2000dp")` so it doesn't clamp                                                                                                                                                                                       |
| `isAttachedToWindow`                      | actually attach it (content view or `WindowManager.addView`)                                                                                                                                                                                                          |

### Visibility & focus

| Instead of                                  | Do                                                                                                                                             |
| ------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| `isShown()` stub                            | real attached view; control via `setVisibility` on the view or its parent container                                                            |
| `hasFocus/isFocused/requestFocus` recording | `setFocusable(true)` (+ `setFocusableInTouchMode`), `requestFocus()`, assert `isFocused()` / `hasFocus()` on views inside a `setup()` Activity |
| "lose focus"                                | move focus to a sibling; `clearFocus()` may hand focus straight back                                                                           |
| `activity.getCurrentFocus()`                | `shadowOf(activity).setCurrentFocus(view)` (ShadowActivity overrides it, so real focus isn't reflected)                                        |

### Clicks, listeners, observers

| Instead of `verify(view)...`              | Do                                                                                                                                                                                |
| ----------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `setOnClickListener(...)`                 | `view.hasOnClickListeners()`, `view.performClick()`, `shadowOf(view).getOnClickListener()`                                                                                        |
| `addOnLayoutChangeListener`               | `shadowOf(view).getOnLayoutChangeListeners()`; trigger with `forceLayout()` + `measure()` + `layout()`                                                                            |
| ViewTreeObserver global-layout / pre-draw | `view.getViewTreeObserver().dispatchOnGlobalLayout()` / `dispatchOnPreDraw()` on the real observer; check **behaviour** (e.g. listener removed ⇒ a second dispatch has no effect) |
| `removeOn...Listener` was called          | behavioural: fire the event again and assert nothing happens                                                                                                                      |
| `performHapticFeedback(c)`                | `shadowOf(view).lastHapticFeedbackPerformed()`                                                                                                                                    |
| `announceForAccessibility`                | `shadowOf(accessibilityManager).setEnabled(true)` then `getSentAccessibilityEvents()` filtered to `TYPE_ANNOUNCEMENT`                                                             |
| attach-state listener removed             | really detach (`parent.removeView`) / re-attach and assert behaviour                                                                                                              |
| `post(Runnable)` sync hack                | `ShadowLooper.idleMainLooper()` / `RobolectricUtil.runAllBackgroundAndUi()`                                                                                                       |

### Visual state & animations

| Instead of recording setters                   | Do                                                                                                                 |
| ---------------------------------------------- | ------------------------------------------------------------------------------------------------------------------ |
| `setAlpha/setScale/setTranslation/setRotation` | assert `getAlpha()` etc.; for animators use `setCurrentFraction(f)` for midpoint/end and check state after `end()` |
| make "reset to 1" meaningful                   | preset the start value to something else (e.g. alpha 0.5)                                                          |
| `setPadding` / `setPaddingRelative`            | preset a sentinel padding, assert it's unchanged or reset (call counts are an implementation detail)               |
| `setBackgroundColor`                           | `((ColorDrawable) view.getBackground()).getColor()`                                                                |
| `setImageBitmap`                               | `((BitmapDrawable) image.getDrawable()).getBitmap()`                                                               |
| `addView/removeView`                           | `getChildCount()`, `getChildAt(i)`, `child.getParent()`                                                            |
| `ViewStub.inflate()`                           | put the stub in a real parent with a real layout resource; assert the inflated view is present                     |
| system gesture exclusion                       | preset a sentinel list; assert real `getSystemGestureExclusionRects()`                                             |

### RecyclerView / ListView

Never fake `getChildAt`, `getChildCount`, `getChildAdapterPosition`,
`findViewHolderForAdapterPosition`, `findChildViewUnder`, scroll offsets.

- Real `RecyclerView` + real `LinearLayoutManager`/`GridLayoutManager` + a tiny
  test `Adapter` (one view type per position, or pre-built views/holders).
  `measure()` + `layout()` it (or attach to a `setup()` Activity).
- Specific item bounds: a small custom `LayoutManager` (e.g.
  `FixedBoundsLayoutManager`) that places child *i* at a given rect — it's not a
  View, so it's allowed.
- Scroll offset: real `scrollBy()` / `scrollToPositionWithOffset()`, or a
  LayoutManager subclass/mock overriding `computeVerticalScrollOffset`.
- Scroll state: `smoothScrollBy()` ⇒ SETTLING, `stopScroll()` ⇒ IDLE (DRAGGING
  needs touch events).
- Verifying `scrollToPosition(n)`: `spy(new LinearLayoutManager(ctx))` +
  `verify(lm).scrollToPosition(n)` is fine (LayoutManager isn't a View).
- "Adapter has N items but only M attached": size the RecyclerView so only M
  fit, assert `getChildCount()`.
- Child ordering: `bringChildToFront`.
- ListView: real `ArrayAdapter`, attach, `setSelection(i)` + idle, assert
  `getFirstVisiblePosition()`; `Shadows.shadowOf(listView).populateItems()`.

### Activity / system services

| Instead of overriding/mocking  | Do                                                                                                                                                                                                                     |
| ------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `getPackageManager()` mock     | `shadowOf(context.getPackageManager())`: `installPackage`, `addOrUpdateActivity`, `addActivityIfNotPresent` + `addIntentFilterForActivity` (real IntentFilter matching), `addResolveInfoForIntent`, `setSystemFeature` |
| `startActivity` verify         | `shadowOf(activity).getNextStartedActivity()` / `shadowOf(app).getNextStartedActivity()`                                                                                                                               |
| `finish()` verify              | `activity.isFinishing()`; result: `shadowOf(activity).getResultCode()`                                                                                                                                                 |
| `moveTaskToBack`               | `shadowOf(activity).isTaskMovedToBack()`                                                                                                                                                                               |
| multi-window / PiP             | `shadowOf(activity).setInMultiWindowMode(true)`, `activity.isInPictureInPictureMode()`                                                                                                                                 |
| `getWindow()` mock for flags   | real `activity.getWindow().getAttributes().flags & FLAG_SECURE`                                                                                                                                                        |
| PowerManager / ActivityManager | `shadowOf(powerManager).setIsInteractive(..)`, `shadowOf(activityManager).setAppTasks(..)` (AppTask may be a mock)                                                                                                     |
| registered receivers           | `shadowOf(app).getRegisteredReceivers()` + `IntentFilter.hasAction`                                                                                                                                                    |
| Resources dimen mocks          | read the real resource value (expected computed like production does)                                                                                                                                                  |
| OnBackPressed                  | `activity.getOnBackPressedDispatcher()` with a real `ComponentActivity`                                                                                                                                                |

## Judging asserts

Before keeping a custom subclass for an assert, ask:

- Is the assert checking **the behaviour under test**, or an incidental
  framework call (exact call counts, call order between two framework setters,
  "this getter was queried")?
- Is the behaviour already covered by another assert in the same test?
- Can a **behavioural** check replace it (fire the event again, re-layout, check
  end state, check a histogram via `HistogramWatcher`)?

Acceptable trade-offs seen in practice: dropping "getter X was called", dropping
strict ordering between independent setters, checking final state instead of the
exact `addFlags`/`clearFlags` calls. Note any meaningful coverage reduction in
the CL description.

## When a minimal subclass is still justified

Known Robolectric gaps (keep a single-override subclass, with a comment):

- `Activity#getTaskId()` — always 0, no setter (needed for distinct tasks).
- `View#getRootWindowInsets()` — no way to set arbitrary root insets.
- `Activity#getReferrer()` — Robolectric always populates it (can't be null).
- `Activity#setRecentsScreenshotEnabled`, `setPictureInPictureParams` (no
  read-back), `requestDragAndDropPermissions` — not shadowed.
- `View#requestRectangleOnScreen`, `releasePointerCapture` — no observable
  state.
- "`setPadding` skipped when unchanged" — identical values leave no trace.
- Handing production code a mocked `ViewTreeObserver` / `WindowInsetsController`
  (via `getViewTreeObserver()` / `getWindowInsetsController()`) when the test
  must verify add/remove calls on it.
- Geometry real layout can't produce (visible rect decoupled from location).

Even then: remove every other override; prefer the real implementation for
everything else.

## Style & hygiene

- Rename fields that no longer hold mocks: `mMockView` → `mView`, `mSpyActivity`
  → `mActivity`, `createMockView()` → `createView()`.
- Remove `@Rule MockitoRule` if nothing uses Mockito anymore; run
  `git cl format` to drop unused imports (don't remove imports by hand).
- New deps (e.g. using `ContextUtils`) may need BUILD.gn deps like
  `//base:base_java`; drop `//third_party/mockito:mockito_java` if unused.
- Error Prone warnings are errors on bots (e.g. `FieldCanBeLocal`,
  `BooleanLiteral`, `UnusedVariable`) — fix them.
- Add `/* param= */` comments for non-obvious literals in `layout(0, 0, w, h)`
  when it aids readability.

## Verify

```sh
git cl format
tools/autotest.py -C out/Debug --run-all <full/path/to/FooTest.java> ...
```

`--run-all` avoids an interactive prompt when a file maps to multiple test
targets (e.g. chrome_junit_tests and components_junit_tests).
