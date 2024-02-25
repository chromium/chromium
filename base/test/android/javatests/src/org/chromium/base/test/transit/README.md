# Public Transit

Public Transit is a framework for instrumentation tests that models app states,
and transitions between them.

### Metaphor

The metaphor for the framework is that a Public Transit Layer provides tests
with public transit routes to navigate the app using shared code, as opposed to
each test driving its private car (writing its own private code) to set up the
test.

A Public Transit test moves around the app by going from `TransitStation` to
`TransitStation`, and the stations are connected by routes (transition methods).
`TransitStations` are marked by `Elements`, which are recognizable features of
the destination station (features such as Android Views), which the test takes
as evidence that it has arrived and is ready to perform any test-specific
operation, checking or further navigation.

### Structure and layers

Public Transit is structured as follows:

|Layer|Contents|File names|Location|Width (how many files)
|-|-|-|-|-|
|Test Layer|Instrumentation test classes|`*Test.java`|`//chrome/**/javatests`|wide|
|Transit Layer|Concrete `TransitStations`, `StationFacilities`|`*Station.java`, `*Condition.java`, etc.|`//chrome/test/android/javatests`|wide|
|Framework Layer|Public Transit classes|All classes with package `org.chromium.base.test.transit.*`|`//base/test`|narrow|

This directory (//base/test/.../base/test/transit) contains the Framework Layer.


## Framework Features


### State awareness

Public Transit is based on the concepts of `ConditionalStates`, `Conditions` and
`Transitions`, which means:

* Keeping track of the state the app is in, including transitions between
  states.
* Evaluating if transitions are done by checking `Conditions`.
* Giving execution control to the Test Layer only while no transitions are
  happening, so as to reduce flakiness.


### Transition management

A transition is considered done when:
* All **enter Conditions** of a `ConditionalState` being entered are fulfilled
  * When moving between `TransitStations` or entering a `StationFacility`
* All **exit Conditions** of a `ConditionalState` being exited are fulfilled
  * When moving between `TransitStations` or leaving a `StationFacility`
* All extra **transition Conditions** specific to the transition are fulfilled
  * Most transitions don't need to add extra special Conditions.


### Better error messages

If any conditions in a transition are not fulfilled within a timeout, the test
fails and the states of all conditions being waited on is printed out:

```
org.chromium.base.test.transit.TravelException: Did not complete transition from <S1: EntryPageStation> to <S2: NewTabPageStation>
    [...]
    at org.chromium.chrome.test.transit.BasePageStation.openNewIncognitoTabFromMenu(BasePageStation.java:82)
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

Instrumentation tests share code primarily through util functions and test
rules, which are limited to certain areas of the code and not easily located.

Public Transit has the goal of increasing code reuse between test classes that
go through the same test setup and user flow by putting common code in the
Transit Layer:

* conditions to ensure certain states are reached
* triggers for transitions
* Espresso `ViewMatchers` for the same UI elements


## Classes and concepts


### TransitStations

A **`TransitStation`** represents one of the app's "screens", that is, a full
(or mostly full) window view. Only one `TransitStation` can be active at any
time.

For each screen in the app, a concrete implementation of `TransitStation` should
be created in the Transit Layer, implementing:

* **`declareElements()`** declaring the `Views` and other enter/exit conditions
  define this `TransitStation`.
* **transition methods** to travel to other `TransitStations` or to enter
  `StationFacilities`. These methods are synchronous and return a handle to the
  entered `ConditionalState` only after the transition is done and the new
  `ConditionalState` becomes `ACTIVE`.

Example of a concrete `TransitStation`:

```
/** The tab switcher screen, with the tab grid and the tab management toolbar. */
public class TabSwitcherStation extends TransitStation {
    public static final ViewElement NEW_TAB_BUTTON =
            viewElement(withId(R.id.new_tab_button));
    public static final ViewElement INCOGNITO_TOGGLE_TABS =
            viewElement(withId(R.id.incognito_toggle_tabs));

    private final ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;

    public TabSwitcherStation(ChromeTabbedActivityTestRule chromeTabbedActivityTestRule) {
        mChromeTabbedActivityTestRule = chromeTabbedActivityTestRule;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        elements.declareView(NEW_TAB_BUTTON);
        elements.declareView(INCOGNITO_TOGGLE_TABS);
    }

    public NewTabPageStation openNewTabFromButton() {
        recheckEnterConditions();
        NewTabPageStation newTab = new NewTabPageStation(mChromeTabbedActivityTestRule);
        return Trip.travelSync(this, newTab, (e) -> NEW_TAB_BUTTON.perform(click()))
    }
```


### StationsFacilities

A **`StationFacility`** represents things like pop-up menus, dialogs or messages
that are scoped to one of the app's "screens".

Multiple `StationFacilities` may be active at one time besides the active
TransitStation that contains them.

As with `TransitStations`, concrete, app-specific implementations of
StationFacility should be created in the Transit Layer overriding
**`declareElements()`** and **transition methods**.


### ConditionalState

Both `TransitStation` and `StationFacility` are **`ConditionalStates`**, which
means they declare enter and exit conditions as `Elements` and have a linear
lifecycle:

`NEW` -> `TRANSITIONING_TO` -> `ACTIVE` -> `TRANSITIONING_FROM` -> `FINISHED`

Once `FINISHED`, a `ConditionalState` should not be navigated to anymore. If a
test comes back to a previous screen, it should be represented by a new
`TransitStation`.


### Condition

**`Conditions`** are checks performed to ensure a certain transition is
finished.

Common `Condition` subclasses are provided by the Framework Layer (e.g.
`ViewConditions` and `CallbackCondition`), and app-specific Conditions should be
implemented in the TransitLayer extending `UIThreadCondition` or
`InstrumentationThreadConditions`.

An example of app-specific condition:

```
class PageLoadedCondition extends UiThreadCondition {
    private final ChromeTabbedActivityTestRule mChromeTabbedActivityTestRule;
    private Tab mMatchedTab;

    PageLoadedCondition(
            ChromeTabbedActivityTestRule chromeTabbedActivityTestRule) {
        mChromeTabbedActivityTestRule = chromeTabbedActivityTestRule;
    }

    @Override
    public String buildDescription() {
        return "Tab loaded";
    }

    @Override
    public boolean check() {
        Tab tab = mChromeTabbedActivityTestRule.getActivity().getActivityTab();
        if (tab != null
                && !tab.isLoading()
                && !tab.getWebContents().shouldShowLoadingUI()) {
            mMatchedTab = tab;
            return true;
        } else {
            return false;
        }
    }

    public Tab getMatchedTab() {
        return mMatchedTab;
    }
}
```

`Conditions` are split between `UIThreadConditions` and
`InstrumentationThreadConditions`. The framework knows to run the check() of
each condition in the right thread.

`Conditions` can depend on each other. See below as an example
`PageInteractableCondition`, which depends on a Tab matched by
`PageLoadedCondition`:

```
/** Fulfilled when a page is interactable. */
class PageInteractableCondition extends UiThreadCondition {
    private final PageLoadedCondition mPageLoadedCondition;

    PageInteractableCondition(PageLoadedCondition pageLoadedCondition) {
        mPageLoadedCondition = pageLoadedCondition;
    }


    @Override
    public String buildDescription() {
        return "Page interactable";
    }

    @Override
    public boolean check() {
        Tab tab = mPageLoadedCondition.getMatchedTab();
        return tab != null && tab.isUsedInteractable();
    }
}
```


### Transitions

From the point of view of the Test Layer, transitions methods are blocking. When
a `TransitStation` or `StationFacility` is returned by one of those methods, it
is always `ACTIVE` and can be immediately acted upon without further waiting.

Code in the Test Layer contains no explicit waits; the waits are in the
Framework Layer.

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

Transitions between `TransitStations` are done by calling `Trip.travelSync()`.

Transitions into and out of `StationFacilities` are done by calling
`stationFacility.enterSync()` or `stationFacility.leaveSync()`. If the app moves
to another `TransitStation`, any active `StationFacilities` have their exit
conditions added to the transition conditions.


## Workflow


### App behavior changes

Since the Transit Layer reflects what the app looks like and what it does,
changes to the app's behavior - such as which screens exist, UI elements and the
navigation graph - need to be reflected in the Transit Layer.


### The Transit Layer cohesion

The Transit Layer is a directed graph of `TransitStations`. Transit Layer
EntryPoints classes provide the entry points into the graph.

There should not be multiple `TransitStations` that represent the same state,
but different variations of the same screen may be modeled as different
`TransitStations`. The cohesion of this graph is important to maximize code
reuse.


### Partially Public Transit tests

It is possible to write tests that start as a Public Transit test and use the
Transit layer to navigate to a certain point, then "hop off" framework and
continue navigating the app as a regular instrumentation test.

While it is preferable to model all transitions to Transit Layer, a test that
uses Public Transit partially also realizes its benefits partially and there
should be no framework impediment to doing so.

Metaphorically, if there is no public transit to an area, you ride it as close
as possible and continue on foot.


### Ownership of the Transit Layer

The Chrome-specific `TransitStations`, `StationFacilities` and `Conditions` that
comprise the Transit Layer should be owned by the same team responsible for the
related production code.

The exception is the core of the Transit Layer, for example `PageStation`, which
is not owned by specific teams, and will be owned by Clank Build/Code Health.


## Additional Features


### Batching {#batching}

It is recommended to batch PublicTransit tests to reduce runtime and save
CQ/CI resources.

##### How to batch a Public Transit test

1. Add `@Batch(Batch.PER_CLASS)` to the test class.
2. Add the `BatchedPublicTransitRule<>` specifying the home station. The *home
   station* is where each test starts and ends.
3. Get the first station in each test case from a batched entry point, e.g.
   `ChromeTabbedActivityPublicTransitEntrPoints#startOnBlankPageBatched()`.
4. Each test should return to the home station. If a test does not end in the
   home station, it will fail (if it already hasn't) with a descriptive message.
   The following tests will also fail right at the start.

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
* `setOnExceptionCallback()` runs a function when a TravelException is created.
  Useful to print debug information before the test fails and the app is closed.
* `setFreezeOnException()` freezes the test when a TravelException is created.
  useful to see what the screen looks like before the test fails and the app is
  closed.


## Specific Cases


### Back Button Behavior {#backbutton}

A transition triggered by the back button is just like any other transition
method declared in the `TransitStation` or `StationFacility`. Use
`t -> Espresso.pressBack()` as a trigger.
