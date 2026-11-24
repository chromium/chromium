# ![Logo](public_transit.webp) Public Transit Best Practices

Guidance for writing and reviewing Public Transit tests and Transit Layer code.
Read the [README](README.md) first for core concepts and default APIs. For
debugging utilities and timeouts, see [Debugging](debugging.md).

[TOC]

## Transit Layer vs. Test Layer {#layers}

See [Structure and Layers](README.md#layers) for what belongs in each layer.

* **Name test classes `*Test.java`**, not `*PTTest.java`.
* **Prefer existing `Station` accessors over raw Elements or test rules**: e.g.
  `page.getTab()` instead of `page.loadedTabElement.value()`, and
  `station.getActivity()` instead of querying the test rule. Do not add a new
  accessor unless the Element is accessed often; otherwise read the
  `public final` Element field directly.
* **Check for existing Transit Layer code to reuse**: before adding a new
  `Station`, `Facility`, `Condition`, transition, trigger, or
  [`Outcome`](#outcomes), check whether one already exists to reuse or extend.
* **Put reusable transitions, triggers, and `Outcome`s in the Transit Layer**:
  model shared transitions and triggers (`*To()`) on the starting `Station` or
  `Facility` (or `ChromeTriggers`), and shared [`Outcome`s](#outcomes) on the
  relevant `Station`/`Facility` or `Journeys`. A transition specific to one test
  class can stay there, using `TripBuilder` (e.g.
  `page.runOnUiThreadTo(...).enterFacility(...)`) instead of manual waits.
* **Return concrete types**: transition methods must return the concrete
  destination `Station` or `Facility` so routes are discoverable via
  auto-complete. When the destination depends on form factor, return a concrete
  common base class (never `Station<?>`) or offer separate methods (e.g.
  `openBookmarksPhone()` / `openBookmarksTablet()`).
* **Drive the UI through declared Transitions, Triggers, and Elements**:
  trigger real user events (`tabSwitcherButtonElement.clickTo().arriveAt(...)`)
  instead of `runOnUiThreadTo(() -> view.performClick())`, which bypasses
  Espresso and displayed/enabled checks. Declare Elements as
  `public final ViewElement<...>` fields so tests and other Stations can access
  them without boilerplate getters.
* **Expose enter-condition products as Elements, not getters**: if an enter
  `Condition` finds or waits for an object (`WebContents`, `Activity`, or a
  controller), implement `ConditionWithResult<T>` and declare it with
  `declareEnterConditionAsElement(...)` as a `public final Element<T>` (read
  with `value()` while `ACTIVE` and `getFromPast()` after exit). Objects passed
  in through the constructor can remain private fields.

### Method naming: `To()`, verbs, and Outcomes {#method-naming}

* **Only `TripBuilder` factories (Triggers) end in `To()`**: e.g. `clickTo()`,
  `runTo()`, `pressBackTo()`, `loadUrlTo(url)`, `runJsTo(js)`. They execute
  nothing until a terminal call (`arriveAt()`, `enterFacility()`, `reach()`,
  etc.).
* **Full-Trip methods use an imperative verb without `To`**: e.g.
  `loadWebPageProgrammatically(url)`, `openRegularTabSwitcher()`,
  `openNewTab()` (reserve the `To` suffix for `TripBuilder` factories like
  `loadUrlTo(url)`).
* **`Outcome` factories are named for the resulting state**: e.g.
  `newTabGroupUiFlow()`, with Javadoc `Outcome: <end state>`.

## Outcomes: Offering an End State Without Its Trigger {#outcomes}

[`Outcome<R>`] packages the state-changes half of a Trip (`enter`, `exit`,
`waitFor`, `and`, `andExit`, `andWaitFor`, `returning`, plus
`facility.replacedBy(next)`) so callers choose the Trigger and join via
`trip.reach(outcome)` or `outcome.after(trip)`. Prefer returning an `Outcome<R>`
over taking a `TripBuilder` argument, adding `...And()` twin methods, or
returning a `Pair` (use `returning(() -> record)` when a Trip enters multiple
`Facility`s).

```java
/** Outcome: the new tab group creation UI flow begins. */
public static <HostStationT extends ...> Outcome<NewTabGroupDialogFacility<HostStationT>>
        newTabGroupUiFlow() {
    SoftKeyboardFacility softKeyboard = new SoftKeyboardFacility();
    return Outcome.enter(new NewTabGroupDialogFacility<HostStationT>(softKeyboard))
            .and(softKeyboard);
}

// Callers choose the Trigger and attach path-specific state changes:
return mNewTabGroup.scrollToAndSelectTo()
        .reach(Journeys.<HostStationT>newTabGroupUiFlow().andExit(mParentAppMenu));
```

See also `CtaPageStation#nextTabSelected()`,
`RegularTabSwitcherStation.afterClosingTab()`, and `TabStripFacility#settled()`.

* **One-shot**: holds `NEW` States/Facilities and can only be reached once.
  Construct a new instance per call; never store one in a field.
* **Built before the Trigger runs**: destination must not depend on state
  mutated by the Trigger (e.g. tab count before closing a tab).
* **Named as the resulting state**: see [Method naming](#method-naming).
* **Scope Conditions by path dependence**: facts true on every entry belong in
  the State's enter `Condition`s; path-specific facts belong in the `Outcome`
  via `andWaitFor(...)`.
* **Keep one-off Trips simple**: direct `TripBuilder` shortcuts (`arriveAt()`,
  `enterFacility()`, `replaceFacility()`, `exitFacilityAnd()`) remain fine when
  only one Trigger leads to the end state.

[`Outcome<R>`]: https://source.chromium.org/search?q=symbol:org.chromium.base.test.transit.Outcome&ss=chromium

## Elements and Expectations {#scoping}

See [Elements](README.md#elements) for default scoping, varargs `declareView`,
`RootSpec`, pending layout checks, `OptionalViewElement`, and `HtmlElement`.

* **Avoid `ViewElement.unscopedOption()` on transient UI**: use it **only** for
  Views that persist across state exits (e.g. a shared toolbar or decor
  container) — never on dialogs, panels, scrims, dropdowns, or menus. Manually
  asserting `assertFalse(coordinator.isVisible())` after dismissal is a sign the
  `Facility`'s Elements should have been scoped.
* **Let `DisplayedCondition` handle layout**: only add
  `ViewElement.initialSettleTimeOption(ms)` when a View's screen bounds animate
  across frames.
* **HTML DOM elements**: subclass `WebPageFacility`
  (`//content/public/test/android:content_transit_java`) and attach it via
  `BasePageStation.Builder#withFacility(facility)` instead of polling with
  `JavaScriptUtils` or `DOMUtils`.
* **Flag-gated UI**: declare conditional affordances with
  `declareOptionalView(...)` and `assert ChromeFeatureList.sFoo.isEnabled()` in
  the transition method so invalid flag states fail immediately instead of
  timing out.
* **Destination expectations**: configure builders with expected state (e.g.
  `withExpectedTitle()`, `withExpectedUrlSubstring()`) so Public Transit polls
  and verifies it as an enter `Condition` instead of asserting after arrival.

## Batching and Test Lifecycle {#batching}

See [Batching](README.md#batching) for choosing a test rule.

* Omit `@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})` on
  tests that start `ChromeTabbedActivity` solely through standard
  `ChromeTransitTestRules` entry points (`AutoResetCtaTransitTestRule`,
  `FreshCtaTransitTestRule`, or `ReusedCtaTransitTestRule`);
  `ChromeTabbedActivityEntryPoints` already marks first-run complete and
  `ChromeActivityTestRule` appends `DISABLE_STARTUP_PROMOS` by default.
* Never call `ActivityFinisher.finishAll()` or manually finish Activities in
  `@After`; doing so breaks batched execution.
* Do not poll `getTabModelSelector().isTabStateInitialized()` in `@Before` after
  `startOnBlankPage()`; the entry point already guarantees initialization.

## Custom Conditions and `waitForAnd()` {#custom-conditions}

When waiting for asynchronous UI-thread events (e.g. `PageLoadMetrics.Observer`
or tab/compositor observers), subclass `UiThreadCondition` (or use
`UiThreadCondition.from(description, check)`) and attach it with `waitForAnd()`.
Avoid `CriteriaHelper.pollInstrumentationThread()` +
`ThreadUtils.runOnUiThreadBlocking()`, blocking `JavaScriptUtils` calls, or
`CountDownLatch`es.

Because both UI-thread observers and `UiThreadCondition#checkWithSuppliers()`
run on the browser UI thread, observer state can be read without `volatile`
fields or thread hopping. Use `dependOnSupplier()` for destination Elements that
do not exist yet (see [Conditions](README.md#conditions)). For in-page actions
that stay on the current `Station`, use
`mPage.runOnUiThreadTo(...).waitForAnd(UiThreadCondition.from(...)).complete()`.

## New Windows, Rotation, and Finishing Activities {#new-task}

Use `inNewTask().arriveAt(...)` instead of polling
`ApplicationStatus.getRunningActivities()` or `isTabStateInitialized()` (call
`allowSubclasses()` on the destination `ActivityElement` when
`ChromeTabbedActivity2` may launch on pre-S).

* **Do not run a nested `Trip` inside an `inNewTask()` trigger**:
  `ActivityExistsInNewTaskCondition` reads `TrafficControl.getActiveStations()`
  live on every poll. If the trigger invokes a helper that enters its own
  `Station` on the new window, the outer `inNewTask()` condition sees that task
  as already owned and times out. Launch the window directly in the trigger.
* **Closing windows / finishing Activities**: call `station.finishActivity()` or
  `trigger.reachLastStop()` (or `reachLastStopAnd(station).complete()`), which
  sets `activityElement.expectActivityDestroyed()`.
* **Orientation changes**: use `station.rotateTo(Configuration.ORIENTATION_...)`
  instead of manual `setRequestedOrientation()` calls.

Keep the number of concurrent windows a test leaves open as low as the scenario
allows.

## `*Fast()` Helpers {#fast-openers}

Stations expose programmatic helpers (`openNewTabFast()`,
`openNewIncognitoTabOrWindowFast()`, `closeCurrentTabFast()`,
`closeLastTabToTabSwitcherFast()`, plus `ChromeTriggers.closeCurrentTabTo(...)`)
that skip driving the UI.

* **Use** for setting up or tearing down preconditions of another feature.
* **Avoid** bypassing the UI affordance under test (e.g. replacing
  `openAppMenu().openNewIncognitoTabOrWindow()` with
  `openNewIncognitoTabOrWindowFast()` in an app-menu test). Comment temporary
  workarounds with a tracking bug.

## `ViewFinder` (Standalone View Waits) {#viewfinder}

Outside a `Station` or `Facility`, prefer [`ViewFinder`] over
`Espresso.onView()` or `ViewUtils.waitForView()` (see
[README](README.md#viewfinder) for APIs and defaults):

* Statically import `waitForView`, `waitForNoView`, `expectView`,
  `expectNoView`, `withId`, `withText`, and `*Option` helpers.
* Drop redundant defaults: omit `isDisplayed()` inside `allOf(...)` and trailing
  `.check(matches(isDisplayed()))`.
* Use varargs (`waitForView(m1, m2)`) instead of `allOf(...)` when not passing
  `Options`, an `Activity`, or a view class, and
  `MoreViewMatchers.isDescendantOf(view)` instead of
  `isDescendantOfA(is(view))`.
* Use options instead of state matchers: `displayingAtLeastOption(100)`,
  `expectDisabledOption()` / `allowDisabledOption()`,
  `rootSpecOption(RootSpec.*)`, and `waitForNoView(matcher)` /
  `expectInvisibleOption()` / `expectGoneOption()`.
* Act on the returned `ViewPresence` directly instead of the `@Deprecated`
  `.onView()`.

[`ViewFinder`]: https://source.chromium.org/search?q=symbol:org.chromium.base.test.transit.ViewFinder&ss=chromium

## Converting Legacy Test Utils (Hop-On) {#hop-on}

See [Hopping On](README.md#hopping-on) for `hopOnAt()` / `hopOnTo()`. Wrapping
legacy polling in `noopTo().waitFor(...)` enters no `Station` and is **not** a
conversion.

* Replace the util in place and return the destination `Station` (changing
  `void` to a `Station` keeps callers compiling; if multiple types are possible,
  return a common base such as `CtaPageStation`).
* Verify the destination `Station`'s enter `Condition`s cover what the legacy
  util checked across feature flag variants.
* Avoid calling hop-on helpers from tests already holding an `ACTIVE` `Station`
  — both APIs abandon previous `Station`s first, and `hopOnTo()` logs a warning
  when hopping on over an active `Station`. Transition directly from the
  existing `Station` instead.
