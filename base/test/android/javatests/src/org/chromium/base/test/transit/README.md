# Public Transit

Public Transit is a framework for instrumentation tests that models app states,
and transitions between them.

### Metaphor

The metaphor for the framework is that a Public Transit Layer provides tests
with public transit routes to navigate the app using shared code, as opposed to
each test driving its private car (writing its own private code) to set up the
test.

A Public Transit test moves around the app by going from `Station` to `Station`,
and the stations are connected by routes (transition methods). `Stations` are
marked by `Elements`, which are recognizable features of the destination station
(features such as Android Views), which the test takes as evidence that it has
arrived and is ready to perform any test-specific operation, checking or further
navigation.

At a `Station` there are `Facilities` that can be entered, such as menus,
dialogs, or more abstract substates, such as data loaded from disk. Transition
methods are also used to enter and exit those `Facilities`.

The metaphor is not very accurate in that `Stations` and `Facilities` instances
are really snapshots of the app state that the test is expected to reach, so a
user action that changes a selection in a form, for example, would be modeled by
not mutating the dialog's `Facility`, but creating a second instance of the
dialog `Facility` with a property. `Stations` and `Facilities` are close to
being immutable objects.

### Structure and layers

Public Transit is structured as follows:

|Layer|Contents|File names|Location|Width (how many files)
|-|-|-|-|-|
|Test Layer|Instrumentation test classes|`*Test.java`|`//chrome/**/javatests`|wide|
|Transit Layer|Concrete `Stations`, `Facilities`|`*Station.java`, `*Condition.java`, etc.|`//chrome/test/android/javatests`|wide|
|Framework Layer|Public Transit classes|All classes with package `org.chromium.base.test.transit.*`|`//base/test`|narrow|

This directory (//base/test/.../base/test/transit) contains the Framework Layer.

#### Test Layer

The **Test Layer** contains the JUnit test classes with @Test methods. It should
be readable at a high level and delegate the logic that can be shared with other
tests to the to Transit Layer.

Code in the Test Layer that uses the Transit Layer should contain no explicit
waits; the waits should be modeled as transition methods.

An example of Test Layer code:

```
@Test
public void testOpenTabSwitcher() {
    BasePageStation page = mTransitEntryPoints.startOnBlankPage();
    AppMenuFacility appMenu = page.openAppMenu();
    page = appMenu.openNewIncognitoTab();
    TabSwitcherStation tabSwitcher = page.openTabSwitcher();
}
```

Most of the time these transition methods, such as
`BasePageStation#openAppMenu()`, should be in the Transit Layer for sharing with
other tests. Transitions specific to the test can be written in the Test Layer.

#### Transit Layer

The **Transit Layer** contains the app-specific `Stations`, `Faciltiies`,
`Transitions` and `Conditions`, as well as entry points. This is the bulk of the
test code.

The Transit Layer is a representation of what the app looks like in terms of
possible states, and how these states can be navigated.

#### Framework Layer

The **Framework Layer** is the Public Transit library code, which is
app-agnostic. It contains the Public Transit concepts of `Station`,
`Transition`, `Condition`, etc.


## Classes and concepts

### Stations

A **`Station`** represents one of the app's "screens", that is, a full (or
mostly full) window view. Only one `Station` can be active at any time.

For each screen in the app, a concrete implementation of `Station` should be
created in the Transit Layer, implementing:

* **`declareElements()`** declaring the `Views` and other enter/exit conditions
  define this `Station`.
* **transition methods** to travel to other `Stations` or to enter `Facilities`.
  These methods are synchronous and return a handle to the entered
  `ConditionalState` only after the transition is done and the new
  `ConditionalState` becomes `ACTIVE`.

Example of a concrete `Station`:

```
/** The tab switcher screen, with the tab grid and the tab management toolbar. */
public class TabSwitcherStation extends Station {
    public static final ViewElement NEW_TAB_BUTTON =
            viewElement(withId(R.id.new_tab_button));
    public static final ViewElement INCOGNITO_TOGGLE_TABS =
            viewElement(withId(R.id.incognito_toggle_tabs));

    protected ActivityElement<ChromeTabbedActivity> mActivityElement;

    @Override
    public void declareElements(Elements.Builder elements) {
        mActivityElement = elements.declareActivity(ChromeTabbedActivity.class);
        elements.declareView(NEW_TAB_BUTTON);
        elements.declareView(INCOGNITO_TOGGLE_TABS);
    }

    public NewTabPageStation openNewTabFromButton() {
        NewTabPageStation newTab = new NewTabPageStation();
        return travelToSync(this, newTab, () -> NEW_TAB_BUTTON.perform(click()))
    }
}
```


### Facilities

A **`Facility`** represents things like pop-up menus, dialogs or messages that
are scoped to one of the app's "screens".

Multiple `Facilities` may be active at one time besides the active Station that
contains them.

As with `Stations`, concrete, app-specific implementations of Facility should be
created in the Transit Layer overriding **`declareElements()`** and **transition
methods**.


### ConditionalState

Both `Station` and `Facility` are **`ConditionalStates`**, which means they
declare enter and exit conditions as `Elements` and have a linear lifecycle:

`NEW` -> `TRANSITIONING_TO` -> `ACTIVE` -> `TRANSITIONING_FROM` -> `FINISHED`

Once `FINISHED`, a `ConditionalState` should not be navigated to anymore. If a
test comes back to a previous screen, it should be represented by a new
`Station` instance.


### Condition

**`Conditions`** are checks performed to ensure a certain transition is
finished.

Common `Condition` subclasses are provided by the Framework Layer (e.g.
`ViewConditions` and `CallbackCondition`).

##### Custom Conditions

Custom app-specific Conditions should be implemented in the TransitLayer
extending `UIThreadCondition` or `InstrumentationThreadConditions`.

A Condition should implement `checkWithSuppliers()`, which should check the
conditions and return `fulfilled()`, `notFulfilled()` or `awaiting()`.An
optional but encouraged status message can be provided as argument. These
messages are aggregated and printed to logcat with the times they were output in
the transition summary. `whether()` can also be returned as a convenience
method.

Custom Conditions may require a dependency to be checked which might not exist
before the transition's trigger is run. They should take the dependency as a
constructor argument of type `Condition` that implements `Supplier<DependencyT>`
and call `dependOnSupplier()`. The dependency should supply `DependencyT` when
fulfilled. TODO(crbug.com/343244345): Create ConditionWithResult\<T\>.

An example of a custom condition:

```
class PageLoadedCondition extends UiThreadCondition {
    private Supplier<Tab> mTabSupplier;

    PageLoadedCondition(Supplier<Tab> tabSupplier) {
        mTabSupplier = dependOnSupplier(tabSupplier, "Tab");
    }

    @Override
    public String buildDescription() {
        return "Tab loaded";
    }

    @Override
    public ConditionStatus checkWithSuppliers() {
        Tab tab = mActivityTabSupplier.get();

        boolean isLoading = tab.isLoading();
        boolean showLoadingUi = tab.getWebContents().shouldShowLoadingUI();
        return whether(
                !isLoading && !showLoadingUI,
                "isLoading %b, showLoadingUi %b",
                isLoading,
                showLoadingUi);
    }
}
```

`Conditions` are split between `UIThreadConditions` and
`InstrumentationThreadConditions`. The framework knows to run the check() of
each condition in the right thread.


### Transitions

From the point of view of the Test Layer, transitions methods are blocking. When
a `Station` or `Facility` is returned by one of those methods, it is always
`ACTIVE` and can be immediately acted upon without further waiting.

##### API

Transitions between `Stations` are done by calling `travelToSync()`.

Transitions into and out of `Facilities` are done by calling
`enterFacilitySync()`, `exitFacilitySync()` or `swapFacilitySync()`. If the app
moves to another `Station`, any active `Facilities` have their exit conditions
added to the transition conditions.

These methods takes as parameter a `Trigger`, which is the code that should be
run to actually make the app move to the next state. Often this will be a UI
interaction like `() -> BUTTON_ELEMENT.perform(click())`.


##### Enter, exit and transition Conditions


The Conditions of a transition are the aggregation of:
* The **enter Conditions** of a `ConditionalState` being entered.
* The **exit Conditions** of a `ConditionalState` being exited unless the same
  Element is in a state being entered too.
* Any extra **transition Conditions** specific to that transition.
  * Most transitions don't need to add extra special Conditions.


##### Implementation


The way a Transition works is:
1. The states being exited go from Phase `ACTIVE` to `TRANSITIONING_FROM` and
   the states being entered go from Phase `NEW` to `TRANSITIONING_TO`.
2. The `Conditions` to complete the Transition are determined by comparing
   `Elements` of states being exited and the ones being entered.
3. A pre-check is run to ensure at least one of the Conditions is not fulfilled.
4. The provided `Trigger` lambda is run.
5. `ConditionWaiter` polls, checking the Conditions each cycle.
6. If ConditionWaiter times out before all Conditions are fulfilled:
    1. The test fails with an exception that contains the Transition, the status
       of all Conditions, and the stack up until the Test Layer.
7. If the Conditions are all fulfilled:
    1. The states being exited go from Phase `TRANSITIONING_FROM` to `FINISHED`
       and the states being entered go from Phase `TRANSITIONING_TO` to
       `ACTIVE`.
    2. A summary of the Condition statuses is printed to logcat.
    3. The entered ConditionalState, now `ACTIVE`, is returned to the transit
       layer and then to the test layer.


### TransitionOptions

`TransitionOptions` let individual Transitions be customized, adjusting
timeouts, adding retries, or disabling the pre-check.


## Primary Framework Features


### State awareness

Public Transit is based on the concepts of `ConditionalStates`, `Conditions` and
`Transitions`, which means:

* Keeping track of the state the app is in, including transitions between
  states.
* Evaluating if transitions are finished by checking `Conditions`.
* Giving execution control to the Test Layer only while no transitions are
  happening to reduce race conditions.


### Better error messages

If any conditions in a transition are not fulfilled within a timeout, the test
fails and the states of all conditions being waited on is printed out:

```
org.chromium.base.test.transit.TravelException: Did not complete transition from <S1: EntryPageStation> to <S2: NewTabPageStation>
    [...]
    at org.chromium.chrome.test.transit.PageStation.openNewIncognitoTabFromMenu(PageStation.java:82)
    at org.chromium.chrome.browser.toolbar.top.TabSwitcherActionMenuPTTest.testClosingAllRegularTabs_DoNotFinishActivity(TabSwitcherActionMenuPTTest.java:94)
    ... 44 trimmed
Caused by: java.lang.AssertionError: org.chromium.base.test.util.CriteriaNotSatisfiedException: Did not meet all conditions:
    [1] [ENTER] [OK  ] View: (with id: id/tab_switcher_button and is displayed on the screen to the user) {fulfilled after 0~701 ms}
    [2] [ENTER] [OK  ] Receive tab opened callback {fulfilled after 0~701 ms}
    [3] [ENTER] [OK  ] Receive tab selected callback {fulfilled after 0~701 ms}
    [4] [ENTER] [OK  ] Tab loaded {fulfilled after 0~701 ms}
    [5] [ENTER] [FAIL] Page interactable or hidden {unfulfilled after 3746 ms}
    [6] [ENTER] [OK  ] Ntp loaded {fulfilled after 0~701 ms}
```

### Reuse of code between tests

Public Transit increases code reuse between test classes that go through the
same test setup and user flow by putting common code in the Transit Layer,
including:

* Conditions to ensure certain states are reached
* Transition methods to go to other states
* Espresso `ViewMatchers` for the same UI elements

The transition methods shows the "routes" that can be taken to continue from the
current state, increasing discoverability of shared code.


## Additional Framework Features


### Batching {#batching}

It is recommended to batch PublicTransit tests to reduce runtime and save CQ/CI
resources.

##### How to batch a Public Transit test

1. Add `@Batch(Batch.PER_CLASS)` to the test class.
2. Add the `BatchedPublicTransitRule<>` specifying the home station. The *home
   station* is where each test starts and ends.
3. Get the first station in each test case from a batched entry point, e.g.
   `ChromeTabbedActivityPublicTransitEntryPoints#startOnBlankPage()`.
4. Each test should return to the home station. If a test does not end in the
   home station, it will fail (if it already hasn't) with a descriptive message.
   The following tests will also fail right at the start.

In Chrome, in many situations, `BlankCTATabInitialStatePublicTransitRule` is
more practical to use to automatically reset Tab state. It also acts as entry
point.

### Debugging Helpers

##### ViewPrinter

`ViewPrinter` is useful to print a View hierarchy to write ViewElements and
debug failures. The output with default options looks like this:

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

##### PublicTransitConfig

`PublicTransitConfig` configures the test to run differently for local
debugging:

* `setTransitionPauseForDebugging()` causes the test to run more slowly, pausing
  for some time after each transition and displaying a Toast with which Station
  is active. 1500ms is a good default.
* `setOnExceptionCallback()` runs the given callback when an Exception happens
  during a Transition. Useful to print debug information before the test fails
  and the app is closed.
* `setFreezeOnException()` freezes the test when an Exception happens during a
  Transition. Useful to see what the screen looks like before the test fails and
  the instrumented app is closed.


## Workflow


### App behavior changes

Since the Transit Layer reflects what the app looks like and what it does,
changes to the app's behavior - such as which screens exist, UI elements and the
navigation graph - need to be reflected in the Transit Layer.


### The Transit Layer cohesion

The Transit Layer is a directed graph of `Stations`. Transit Layer EntryPoints
classes provide the entry points into the graph.

There should not be multiple `Stations` that represent the same state, but
different variations of the same screen may be modeled as different `Stations`.
The cohesion of this graph is important to maximize code reuse.


### Ownership of the Transit Layer

The Chrome-specific `Stations`, `Facilities` and `Conditions` that comprise the
Transit Layer should be owned by the same team responsible for the related
production code.

The exception is the core of the Transit Layer, for example `PageStation`, which
is not owned by specific teams, and will be owned by Clank Build/Code Health.


## Guidance for Specific Cases


##### Back Button Behavior {#backbutton}

A transition triggered by the back button is just like any other transition
method declared in the `Station` or `Facility`. Use `t -> Espresso.pressBack()`
as a trigger.


##### Partially Public Transit tests

It is possible to write tests that start as a Public Transit test and use the
Transit layer to navigate to a certain point, then "hop off" framework and
continue navigating the app as a regular instrumentation test.

While it is preferable to model all transitions in the Transit Layer, a test
that uses Public Transit partially also realizes its benefits partially and
there should be no framework impediment to doing so.

Metaphorically, if there is no public transit to an address, you ride it as
close as possible and continue on foot.
