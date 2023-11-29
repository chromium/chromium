package org.chromium.chrome.browser.automotive;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
import org.chromium.ui.base.TestActivity;

/** Tests logic in the {@link AutomotiveUtils} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutomotiveUtilsUnitTest {

    @Rule
    public AutomotiveContextWrapperTestRule mAutomotiveContextWrapperTestRule =
            new AutomotiveContextWrapperTestRule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Test
    public void testGetAutomotiveToolbarHeightDp() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            int automotiveToolbarHeightDp =
                                    AutomotiveUtils.getAutomotiveToolbarHeightDp(activity);
                            assertTrue(
                                    "Automotive toolbar should exist on automotive devices.",
                                    automotiveToolbarHeightDp > 0);
                        });

        mAutomotiveContextWrapperTestRule.setIsAutomotive(false);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            int automotiveToolbarHeightDp =
                                    AutomotiveUtils.getAutomotiveToolbarHeightDp(activity);
                            assertEquals(
                                    "Automotive toolbar height should not exist on non automotive"
                                            + " devices.",
                                    0,
                                    automotiveToolbarHeightDp);
                        });
    }
}
