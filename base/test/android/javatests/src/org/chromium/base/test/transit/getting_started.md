# ![Logo](public_transit.webp) Getting Started with Public Transit

This is a guide on writing your first test with Public Transit.

[TOC]

## Create a new test file

This can be in any instrumentation test target. The naming convention is to use
the suffix `PTTest.java`. I'll create as an example
`chrome/android/javatests/src/org/chromium/chrome/browser/MyPTTest.java` and add
it to `chrome/android/chrome_test_java_sources.gni`

If you're using a new `"javatests"` target instead, you need to add BUILD.gn
deps on:

```
"//chrome/test/android:chrome_java_transit",
"//base/test:public_transit_java",
```

A minimal `MyPTTest.java`:
```java
package org.chromium.chrome.browser;

[imports]

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class MyPTTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    // Recommend to batch whenever possible so the test runs faster.
    // Omit this rule in a non-batched test.
    @Rule
    public BatchedPublicTransitRule<PageStation> mBatchedRule =
            new BatchedPublicTransitRule<>(PageStation.class, /* expectResetByTest= */ true);

    ChromeTabbedActivityPublicTransitEntryPoints mEntryPoints =
            new ChromeTabbedActivityPublicTransitEntryPoints(sActivityTestRule);

    @Test
    @LargeTest
    public void testOpenBlankPage() {
        PageStation page = mEntryPoints.startOnBlankPage(mBatchedRule);
        TransitAsserts.assertFinalDestination(page);
    }
}
```

Run it with `$ tools/autotest.py -C out/Debug MyPTTest`

## Modify the Tab Switcher Button Behavior

Let's make a change that we can actually test: modify the tab switcher button to
**display the number of tabs in roman numerals** when up to 10 tabs. We don't
know where to make this change or what type of View the tab switcher button is,
so it's handy to look at the View hierarchy:

```java
public class MyPTTest {
    @Test
    @LargeTest
    public void testOpenBlankPage() {
        [...]

+       ViewPrinter.printActivityDecorView(page.getActivity());
    }
}
```

In logcat, ViewPrinter has dumped the visible Views and we search the output for
`tab_switcher`:

```
╰── @id/toolbar_tablet_layout | LinearLayout
    ├── @id/home_button | HomeButton
    ├── @id/back_button | ChromeImageButton
    ├── @id/forward_button | ChromeImageButton
    ├── @id/refresh_button | ChromeImageButton
    ├── @id/location_bar | LocationBarTablet
    │   ├── @id/location_bar_status | StatusView
    │   │   ╰── @id/location_bar_status_icon_view | StatusIconView
    │   │       ╰── @id/location_bar_status_icon_frame | FrameLayout
    │   │           ╰── @id/location_bar_status_icon | ChromeImageView
    │   ├── "" | @id/url_bar | UrlBarApi26
    │   ╰── @id/url_action_container | LinearLayout
    │       ╰── @id/mic_button | ChromeImageButton
    ├── @id/optional_toolbar_button | ListMenuButton
->  ├── @id/tab_switcher_button | ToggleTabStackButton
    ╰── @id/menu_button_wrapper | MenuButton
        ╰── @id/menu_button | ChromeImageButton
```

We can see that `@id/tab_switcher_button` is a `ToggleTabStackButton`. Opening
`ToggleTabStackButton.java`, we see it uses a `TabSwitcherDrawable` which draws
the number to a Canvas.

Next, Implement the number conversion in the existing
`TabSwitcherDrawable.java`:

```java
public class TabSwitcherDrawable extends TintedDrawable {
    [...]

    private String getTabCountString() {
        if (mTabCount <= 0) {
            return "";
        } else if (mTabCount > 99) {
            return mIncognito ? ";)" : ":D";
        } else {
-           return String.format(Locale.getDefault(), "%d", mTabCount);

+           return switch (mTabCount) {
+               case 1 -> "I";
+               case 2 -> "II";
+               case 3 -> "III";
+               case 4 -> "IV";
+               case 5 -> "V";
+               case 6 -> "VI";
+               case 7 -> "VII";
+               case 8 -> "VIII";
+               case 9 -> "XI";
+               case 10 -> "X";
+               default -> String.format(Locale.getDefault(), "%d", mTabCount);
+           };
        }
    }
```

There are two solid options to verify the rendered numeral with a test:

### Option 1: testOneTab_I_render() as a RenderTest

We can use `RenderTestRule` to render the tab switcher button and compare it against golden images:

```java
public class MyPTTest {
    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_TOOLBAR)
                    .setRevision(0)
                    .build();

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testOneTab_I_render() throws IOException {
        PageStation page = mEntryPoints.startOnBlankPage(mBatchedRule);
        mRenderTestRule.render(page.getTabSwitcherButton(), "1_tab");
        TransitAsserts.assertFinalDestination(page);
    }
}
```

We need the View instance to pass to `mRenderTestRule.render()`. The View can be
retrieved from the Element declared in `PageStation#declareElements()`:

```java
public class PageStation extends Station {
    public static final ViewElement TAB_SWITCHER_BUTTON =
             unscopedViewElement(withId(R.id.tab_switcher_button));

+   Supplier<View> mTabSwitcherButton;

    public void declareElements(Elements.Builder elements) {
        [...]
-       elements.declareView(TAB_SWITCHER_BUTTON);
+       mTabSwitcherButton = elements.declareView(TAB_SWITCHER_BUTTON);
        [...]
    }

+   public ImageButton getTabSwitcherButton() {
+       assertSuppliersCanBeUsed();
+       return (ImageButton) mTabSwitcherButton.get();
+   }
}
```

The render test will fail when run locally (only after running on CQ a golden
can be triaged as a positive so that the test passes). Regardless, we can see
the rendered image in `test_results`:

![Tab switcher button rendering I](button_I.webp)

*`MyPTTest.1_tab.rev_0` produced by `testOneTab_I_render()`*

### Option 2: testOneTab_I() Using a ForTesting Hook

The second option is to intercept the displayed String when it's rendered
(requiring `*ForTesting()` code). We will use this option for the rest of the
guide, but the render test is a very good way of testing this too.

```java
public class MyPTTest {
    public void testOneTab_I() {
        PageStation page = mEntryPoints.startOnBlankPage(mBatchedRule);

        ImageButton tabSwitcherButton = page.getTabSwitcherButton();
        TabSwitcherDrawable tabSwitcherDrawable = (TabSwitcherDrawable) tabSwitcherButton.getDrawable();
        assertEquals("I", tabSwitcherDrawable.getTextRenderedForTesting());
        TransitAsserts.assertFinalDestination(page);
    }
}
```

## testTwoTabs_II() and Batching

Let's add a second test case:

```java
public class MyPTTest {
    public void testTwoTabs_II() {
        PageStation page = mEntryPoints.startOnBlankPage(mBatchedRule);
        NewTabPageStation ntp = page.openGenericAppMenu().openNewTab();

        ImageButton tabSwitcherButton = ntp.getTabSwitcherButton();
        TabSwitcherDrawable tabSwitcherDrawable =
                (TabSwitcherDrawable) tabSwitcherButton.getDrawable();
        assertEquals("II", tabSwitcherDrawable.getTextRenderedForTesting());
        TransitAsserts.assertFinalDestination(ntp);
    }
}
```

Run the whole test file to run both tests and... `testOneTab_I()` fails because
`testTwoTabs_II()` left the app with two tabs rather than one:

```
[FAILURE] org.chromium.chrome.browser.MyPTTest#testOneTab_I:
org.junit.ComparisonFailure: expected:<I[]> but was:<I[I]>
	at org.junit.Assert.assertEquals(Assert.java:117)
	at org.junit.Assert.assertEquals(Assert.java:146)
	at org.chromium.chrome.browser.MyPTTest.testOneTab_I(MyPTTest.java:83)
```

### Option 1: Reset State "Manually"

In batched tests, the order of tests is arbitrary unless `@FixMethodOrder` is used. Additionally, either:

1. Each test must leave the app in a state where the other tests can run or;
2. Each test must reset the app to a state where it can run.

The "manual" fix for the test failure above is closing the second tab opened in
`testTwoTabs_II()`:

```java
public class MyPTTest {
    public void testTwoTabs_II() {
        [...]
-       TransitAsserts.assertFinalDestination(ntp);

+       // Reset tab model for batching
+       page = ntp.openTabSwitcherActionMenu().selectCloseTab(PageStation.class);
+       TransitAsserts.assertFinalDestination(page);
    }
}
```

### Option 2: Reset State with BlankCTATabInitialStatePublicTransitRule

Manually resetting in each test doesn't scale very well in many cases. There are
some shortcuts for undoing state set during a test, and for tabs specifically,
we are going to use `BlankCTATabInitialStatePublicTransitRule`, which resets
Chrome to a single blank page at the start of each test:

```java
public class MyPTTest {
-   @Rule
-   public BatchedPublicTransitRule<PageStation> mBatchedRule =
-           new BatchedPublicTransitRule<>(PageStation.class, /* expectResetByTest= */ true);
-
-   ChromeTabbedActivityPublicTransitEntryPoints mEntryPoints =
-           new ChromeTabbedActivityPublicTransitEntryPoints(sActivityTestRule);

+   @Rule
+   public BlankCTATabInitialStatePublicTransitRule mInitialStateRule =
+       new BlankCTATabInitialStatePublicTransitRule(sActivityTestRule);

    public void testTwoTabs_II() {
-       PageStation page = mEntryPoints.startOnBlankPage(mBatchedRule);

+       PageStation page = mInitialStateRule.startOnBlankPage();
        [...]
    }
}
```

## testFiveTabs_V() and Journeys

How do we test more tabs? We could create a loop that opens the desired number
of tabs, but `Journeys` is a handy, faster shortcut.

```java
public class MyPTTest {
    public void testFiveTabs_V() {
        PageStation page = mInitialStateRule.startOnBlankPage();
        page = Journeys.prepareTabs(page, 5, 0, "about:blank");

        ImageButton tabSwitcherButton = page.getTabSwitcherButton();
        TabSwitcherDrawable tabSwitcherDrawable =
            (TabSwitcherDrawable) tabSwitcherButton.getDrawable();
        assertEquals("V", tabSwitcherDrawable.getTextRenderedForTesting());
        TransitAsserts.assertFinalDestination(page);
    }
}
```

## Creating TabSwitcherButtonFacility

Facilities are a way to model parts of the app without changing `Stations`. They
are useful when the state they model is optional, or when the state is
interesting to only a small number of tests.

Let's make a `TabSwitcherButtonFacility` representing the button we're testing.
`chrome/test/android/javatests/src/org/chromium/chrome/test/transit/` is where
Chrome's Transit Layer is located, so create `TabSwitcherButtonFacility.java`
there:

```java
package org.chromium.chrome.test.transit;

[imports]

public class TabSwitcherButtonFacility extends Facility<PageStation> {
    private Supplier<View> mTabSwitcherButton;

    @Override
    public void declareElements(Elements.Builder elements) {
        mTabSwitcherButton = elements.declareView(PageStation.TAB_SWITCHER_BUTTON);
    }

    public ImageButton getView() {
        assertSuppliersCanBeUsed();
        return (ImageButton) mTabSwitcherButton.get();
    }

    public String getTextRendered() {
        TabSwitcherDrawable tabSwitcherDrawable = (TabSwitcherDrawable) getView().getDrawable();
        return tabSwitcherDrawable.getTextRenderedForTesting();
    }
}
```

### Create a Transition Method

`TabSwitcherButtonFacility` declares a ViewElement in `declareElements()`, which
means the Facility is considered active only after a View
`withId(R.id.tab_switcher_button)` is fully displayed.

We then change PageStation to connect it to TabSwitcherButtonFacility through a
synchronous transition method `focusOnTabSwitcherButton()`, which creates the
destination TabSwitcherButtonFacility, triggers a transition to it, and returns
after all its Enter Conditions are met:

```java
public class PageStation extends Station {
-   public ImageButton getTabSwitcherButton() {
-       assertSuppliersCanBeUsed();
-       return (ImageButton) mTabSwitcherButton.get();
-   }

+   public TabSwitcherButtonFacility focusOnTabSwitcherButton() {
+       return enterFacilitySync(new TabSwitcherButtonFacility(), /* trigger= */ null);
+   }
}
```

The trigger is null since we expect no input to be necessary for the Conditions
to be fulfilled; they should already be fulfilled.

After calling the transition method `focusOnTabSwitcherButton()`, as soon as
this View is fully displayed, the transition is completed and the active
Facility is returned to the test, ready to be used:

```java
public class MyPTTest {
    public void testOneTab_I() {
        PageStation page = mInitialStateRule.startOnBlankPage();

        TabSwitcherButtonFacility tabSwitcherButton = page.focusOnTabSwitcherButton();
        assertEquals("I", tabSwitcherButton.getTextRendered());
        TransitAsserts.assertFinalDestination(page);
    }
}
```

It's debatable if the Facility is warranted here just to encapsulate the logic
of getting the rendered text. To wait on the rendered text with a Condition,
though, a Facility is necessary. Let's create this Condition.

## Adding a Custom Condition

```java
public class TabSwitcherButtonFacility extends Facility<PageStation> {
+   private final String mExpectedText;
    private Supplier<View> mTabSwitcherButton;

+   public TabSwitcherButtonFacility(String expectedText) {
+       mExpectedText = expectedText;
    }

    @Override
    public void declareElements(Elements.Builder elements) {
        mTabSwitcherButton = elements.declareView(PageStation.TAB_SWITCHER_BUTTON);
+       elements.declareEnterCondition(new TextRenderedCondition());
    }

+   private class TextRenderedCondition extends Condition {
+       public TextRenderedCondition() {
+           super(/* isRunOnUiThread= */ true);
+           dependOnSupplier(mTabSwitcherButton, "ButtonView");
+       }
+
+       @Override
+       protected ConditionStatus checkWithSuppliers() {
+           ImageButton button = (ImageButton) mTabSwitcherButton.get();
+           TabSwitcherDrawable tabSwitcherDrawable = (TabSwitcherDrawable) button.getDrawable();
+           String renderedText = tabSwitcherDrawable.getTextRenderedForTesting();
+           return whether(mExpectedText.equals(renderedText), "expected=%s actual=%s", mExpectedText, renderedText);
+       }
+
+       @Override
+       public String buildDescription() {
+           return "Rendered text is " + mExpectedText;
+       }
    }
}
```

`focusOnTabSwitcherButton()` now needs to be passed the expected text:

```java
public class PageStation extends Station {
+   public TabSwitcherButtonFacility focusOnTabSwitcherButton(String expectedText) {
+       return enterFacilitySync(new TabSwitcherButtonFacility(expectedText), /* trigger= */ null);
+   }
}
```

And since the assert from the Test Layer became a Condition, we remove the
assert:

```java
public class MyPTTest {
    public void testOneTab_I() {
        PageStation page = mInitialStateRule.startOnBlankPage();
        TabSwitcherButtonFacility tabSwitcherButton = page.focusOnTabSwitcherButton("I");
+       TransitAsserts.assertFinalDestination(page);
    }
}
```

We can verify the Condition is working by running the test and looking at the
logcat.

```
FacilityCheckIn 14 (enter <S9|F1005: TabSwitcherButtonFacility>): Conditions fulfilled:
    [1] [ENTER] [OK  ] View: (view.getId() is <2130774242/org.chromium.chrome.tests:id/tab_switcher_button> and (getGlobalVisibleRect() > 90%)) {fulfilled after 0~1 ms}
    [2] [ENTER] [OK  ] Rendered text is I {fulfilled after 0~2 ms}
            2-    4ms (  2x): OK   | expected=I actual=I
```

## Adding Transition Methods to TabSwitcherButtonFacility

If we wanted to move the transition methods involving the tab switcher button
from PageStation into TabSwitcherButtonFacility, we can do so to illustrate the
case where Facilities have transition methods:

```java
public class TabSwitcherButtonFacility extends Facility<PageStation> {
+   public HubTabSwitcherStation clickToOpenHub() {
+       return mHostStation.travelToSync(new HubTabSwitcherStation(), () -> PageStation.TAB_SWITCHER_BUTTON.perform(click()));
+   }
+
+   public TabSwitcherActionMenuFacility longClickToOpenActionMenu() {
+       return mHostStation.enterFacilitySync(new TabSwitcherActionMenuFacility(), () -> PageStation.TAB_SWITCHER_BUTTON.perform(longClick()));
+   }
}
```

## Creating Stations

Creating a Station is very similar to creating a Facility, and a less common
operation, so I won't cover it in this guide. The steps are analogous to a
Facility:

1. Create a concrete class `MyStation` the extends `Station`.
2. Fill `declareElements()` in `MyStation` with the Elements/Conditions to
   recognize it's active and ready to be interacted with.
3. Create a transition method from somewhere in the Transit Layer to navigate to
   `MyStation` that returns an instance of `MyStation`.
4. Add accessors for its Elements and transition methods to other
   Stations/Facilities as necessary for tests.

## More on Public Transit

While this guide is written in a codelab style, I highly recommend reading the
rest of the documentation in the [README.md](README.md). It contains more
information on the architecture, motivation and features of Public Transit.
