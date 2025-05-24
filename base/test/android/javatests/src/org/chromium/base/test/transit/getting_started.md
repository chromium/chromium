# ![Logo](public_transit.webp) Getting Started with Public Transit

This is a guide on writing your first test with Public Transit.

[TOC]

## Create a new test file

This can be in any instrumentation test target. The naming convention is to use
the suffix `PTTest.java`. I'll create as an example
`chrome/android/javatests/src/org/chromium/chrome/browser/MyPTTest.java` and add
it to `chrome/android/javatests/BUILD.gn`

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
@Batch(Batch.PER_CLASS)  // Batching is recommended for faster tests.
public class MyPTTest {
    // Reuse the Activity between test cases when possible so the batched test
    // runs faster.
    @Rule
    public ReusedCtaTransitTestRule<WebPageStation> mCtaTestRule =
            ChromeTransitTestRules.blankPageStartReusedActivityRule();

    @Test
    @LargeTest
    public void testOpenBlankPage() {
        PageStation page = mCtaTestRule.start();
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
        PageStation page = mCtaTestRule.start();
        mRenderTestRule.render(page.tabSwitcherButtonElement.get(), "1_tab");
        TransitAsserts.assertFinalDestination(page);
    }
}
```

We need the View instance to pass to `mRenderTestRule.render()`. The View can be
retrieved from the Element already declared in `PageStation`:

```java
public class PageStation extends Station {
    public ViewElement<ToggleTabStackButton> tabSwitcherButtonElement;

    public PageStation() {
        [...]
        tabSwitcherButtonElement =
                declareView(ToggleTabStackButton.class, withId(R.id.tab_switcher_button));
        [...]
    }
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
        PageStation page = mCtaTestRule.start();

        ImageButton tabSwitcherButton = page.tabSwitcherButtonElement.get();
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
        PageStation page = mCtaTestRule.start();
        NewTabPageStation ntp = page.openGenericAppMenu().openNewTab();

        ImageButton tabSwitcherButton = ntp.tabSwitcherButtonElement.get();
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

### Option 2: Reset State with AutoResetCtaTransitTestRule

Manually resetting in each test doesn't scale very well in many cases. There are
some shortcuts for undoing state set during a test, and for tabs specifically,
we are going to use `AutoResetCtaTransitTestRule`, which resets
Chrome to a single blank page at the start of each test:

```java
public class MyPTTest {
-   @Rule
-   public ReusedCtaTransitTestRule<WebPageStation> mCtaTestRule =
-           ChromeTransitTestRules.blankPageStartReusedActivityRule();

+   @Rule
+   public AutoResetCtaTransitTestRule mCtaTestRule =
+       new ChromeTransitTestRules.autoResetCtaActivityRule();

    public void testTwoTabs_II() {
-       PageStation page = mCtaTestRule.start();

+       PageStation page = mCtaTestRule.startOnBlankPage();
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
        PageStation page = mCtaTestRule.startOnBlankPage();
        page = Journeys.prepareTabs(page, 5, 0, "about:blank");

        ImageButton tabSwitcherButton = page.tabSwitcherButtonElement.get();
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

Just for the sake of this guide, let's make a `TabSwitcherButtonFacility`
representing the button we're testing.
`chrome/test/android/javatests/src/org/chromium/chrome/test/transit/` is where
Chrome's Transit Layer is located, so create `TabSwitcherButtonFacility.java`
there:

```java
package org.chromium.chrome.test.transit;

[imports]

public class TabSwitcherButtonFacility extends Facility<PageStation> {
    public ViewElement<ToggleTabStackButton> buttonElement;

    public TabSwitcherButtonFacility() {
        buttonElement =
                declareView(ToggleTabStackButton.class, withId(R.id.tab_switcher_button)));
    }

    public String getTextRendered() {
        TabSwitcherDrawable tabSwitcherDrawable =
                (TabSwitcherDrawable) buttonElement.get().getDrawable();
        return tabSwitcherDrawable.getTextRenderedForTesting();
    }
}
```

### Create a Transition Method

`TabSwitcherButtonFacility` declares a ViewElement, which means the Facility is
considered active only after a View `withId(R.id.tab_switcher_button)` is fully
displayed.

We then change PageStation to connect it to TabSwitcherButtonFacility through a
synchronous transition method `focusOnTabSwitcherButton()`, which creates the
destination TabSwitcherButtonFacility, triggers a transition to it, and returns
after all its Enter Conditions are met:

```java
public class PageStation extends Station {
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
        PageStation page = mCtaTestRule.startOnBlankPage();

        TabSwitcherButtonFacility tabSwitcherButton = page.focusOnTabSwitcherButton();
        assertEquals("I", tabSwitcherButton.getTextRendered());
        TransitAsserts.assertFinalDestination(page);
    }
}
```

The Facility is not really warranted here just to encapsulate the logic of
getting the rendered text. To wait on the rendered text with a Condition, you
can subclass Condition or use SimpleConditions. For illustration, let's create a
subclass, which is recommended for more complex Conditions:

## Adding a Custom Condition

```java
public class TabSwitcherButtonFacility extends Facility<PageStation> {
    public ViewElement<ToggleTabStackButton> buttonElement;

    public TabSwitcherButtonFacility(String expectedText) {
        buttonElement =
                declareView(ToggleTabStackButton.class, withId(R.id.tab_switcher_button));
+       declareEnterCondition(new TextRenderedCondition(expectedText));
    }

+   private class TextRenderedCondition extends Condition {
+       private final String mExpectedText;
+
+       public TextRenderedCondition(String expectedText) {
+           super(/* isRunOnUiThread= */ true);
+           dependOnSupplier(buttonElement, "ButtonView");
+           mExpectedText = expectedText;
+       }
+
+       @Override
+       protected ConditionStatus checkWithSuppliers() {
+           ImageButton button = (ImageButton) buttonElement.get();
+           TabSwitcherDrawable tabSwitcherDrawable = (TabSwitcherDrawable) button.getDrawable();
+           String renderedText = tabSwitcherDrawable.getTextRenderedForTesting();
+           return whetherEquals(mExpectedText, renderedText);
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
        PageStation page = mCtaTestRule.startOnBlankPage();
        page.focusOnTabSwitcherButton("I");
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
+       return mHostStation.travelToSync(
+               new HubTabSwitcherStation(), buttonElement.getClickTrigger());
+   }
+
+   public TabSwitcherActionMenuFacility longClickToOpenActionMenu() {
+       return mHostStation.enterFacilitySync(
+               new TabSwitcherActionMenuFacility(), buttonElement.getLongPressTrigger());
+   }
}
```

## Creating Stations

Creating a Station is very similar to creating a Facility, and a less common
operation, so I won't cover it in this guide. The steps are analogous to a
Facility:

1. Create a concrete class `MyStation` the extends `Station`.
2. Use `declareView()`, `declareCondition()`, etc. in `MyStation`'s constructor
   with the Elements/Conditions to recognize it's active and ready to be
   interacted with.
3. Create a transition method from somewhere in the Transit Layer to navigate to
   `MyStation` that returns an instance of `MyStation`.
4. Add transition methods to other Stations/Facilities as necessary for tests.

## More on Public Transit

While this guide is written in a codelab style, I highly recommend reading the
rest of the documentation in the [README.md](README.md). It contains more
information on the architecture, motivation and features of Public Transit.
