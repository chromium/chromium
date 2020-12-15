// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.graphics.Bitmap;
import android.view.View;
import android.widget.ListPopupWindow;
import android.widget.ListView;
import android.widget.TextView;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.test.mock.MockNavigationController;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

import java.util.concurrent.ExecutionException;

/**
 * Tests for the navigation popup.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class NavigationPopupTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final int INVALID_NAVIGATION_INDEX = -1;

    private Profile mProfile;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        // TODO (https://crbug.com/1063807):  Add incognito mode tests.
        TestThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> mProfile = Profile.getLastUsedRegularProfile());
    }

    // Exists solely to expose protected methods to this test.
    private static class TestNavigationEntry extends NavigationEntry {
        public TestNavigationEntry(int index, GURL url, GURL virtualUrl, GURL originalUrl,
                String title, Bitmap favicon, int transition, long timestamp) {
            super(index, url, virtualUrl, originalUrl, GURL.emptyGURL(), title, favicon, transition,
                    timestamp);
        }
    }

    private static class TestNavigationController extends MockNavigationController {
        private final NavigationHistory mHistory;
        private int mNavigatedIndex = INVALID_NAVIGATION_INDEX;

        public TestNavigationController() {
            mHistory = new NavigationHistory();
            mHistory.addEntry(new TestNavigationEntry(
                    1, new GURL("about:blank"), GURL.emptyGURL(), null, "About Blank", null, 0, 0));
            mHistory.addEntry(new TestNavigationEntry(5,
                    new GURL(UrlUtils.encodeHtmlDataUri("<html>1</html>")), GURL.emptyGURL(),
                    GURL.emptyGURL(), null, null, 0, 0));
        }

        @Override
        public NavigationHistory getDirectedNavigationHistory(boolean isForward, int itemLimit) {
            return mHistory;
        }

        @Override
        public void goToNavigationIndex(int index) {
            mNavigatedIndex = index;
        }
    }

    @Test
    @MediumTest
    @Feature({"Navigation"})
    public void testFaviconFetching() throws ExecutionException {
        final TestNavigationController controller = new TestNavigationController();
        final ListPopupWindow popup = showPopup(controller);

        CriteriaHelper.pollUiThread(() -> {
            NavigationHistory history = controller.mHistory;
            for (int i = 0; i < history.getEntryCount(); i++) {
                Criteria.checkThat("Favicon[" + i + "] not updated",
                        history.getEntryAtIndex(i).getFavicon(), Matchers.notNullValue());
            }
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> popup.dismiss());
    }

    @Test
    @SmallTest
    @Feature({"Navigation"})
    public void testItemSelection() throws ExecutionException {
        final TestNavigationController controller = new TestNavigationController();
        final ListPopupWindow popup = showPopup(controller);

        TestThreadUtils.runOnUiThreadBlocking((Runnable) () -> popup.performItemClick(1));

        Assert.assertFalse("Popup did not hide as expected.", popup.isShowing());
        Assert.assertEquals(
                "Popup attempted to navigate to the wrong index", 5, controller.mNavigatedIndex);
    }

    @Test
    @SmallTest
    @Feature({"Navigation"})
    public void testShowAllHistory() throws ExecutionException {
        final TestNavigationController controller = new TestNavigationController();
        final ListPopupWindow popup = showPopup(controller);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ListView list = popup.getListView();
            View view = list.getAdapter().getView(list.getAdapter().getCount() - 1, null, list);
            TextView text = (TextView) view.findViewById(R.id.entry_title);
            Assert.assertNotNull(text);
            Assert.assertEquals(text.getResources().getString(R.string.show_full_history),
                    text.getText().toString());
        });
    }

    private ListPopupWindow showPopup(NavigationController controller) throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            NavigationPopup popup = new NavigationPopup(mProfile, mActivityTestRule.getActivity(),
                    controller, NavigationPopup.Type.TABLET_FORWARD);
            popup.show(mActivityTestRule.getActivity()
                               .getToolbarManager()
                               .getToolbarLayoutForTesting());
            return popup.getPopupForTesting();
        });
    }

}
