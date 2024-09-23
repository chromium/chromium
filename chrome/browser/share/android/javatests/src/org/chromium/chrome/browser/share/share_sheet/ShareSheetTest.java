// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.PackageManagerWrapper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.share.ShareHistoryBridge;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * The fixture for share sheet tests, which sets up mock system state and implements some utility
 * methods for writing these tests.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Batch(Batch.PER_CLASS)
public class ShareSheetTest {
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private Profile mProfile;
    private List<ResolveInfo> mAvailableResolveInfos;

    // foo.bar.baz -> baz
    private String labelFromPackageName(String packageName) {
        assert packageName.contains(".");
        return packageName.substring(packageName.lastIndexOf('.') + 1);
    }

    private String packageNameFromLabel(String label) {
        assert !label.contains(".");
        return "org.chromium." + label;
    }

    // baz -> foo.bar.baz/foo.bar.baz
    private String historyNameFromLabel(String label) {
        return packageNameFromLabel(label) + "/" + packageNameFromLabel(label);
    }

    // Produce a stub ResolveInfo with the given package name and activity name.
    // The ResolveInfo is very incomplete - only enough of it is filled in for
    // the specific code under test. Do not pass it to system APIs!
    private ResolveInfo createStubResolveInfo(String packageName) {
        // Do not pass a history entry name to this method - it just takes a
        // package name.
        assert !packageName.contains("/");
        assert packageName.contains(".");

        ResolveInfo resolveInfo = mock(ResolveInfo.class);
        ActivityInfo activityInfo = mock(ActivityInfo.class);

        activityInfo.packageName = packageName;
        activityInfo.name = packageName;

        resolveInfo.activityInfo = activityInfo;

        // There must be a nonzero icon resource, or the platform code that ends
        // up calling loadIcon() will try a fallback path that ends up crashing
        // because the ResolveInfo is a stub.
        resolveInfo.activityInfo.icon = R.drawable.sharing_more;
        resolveInfo.icon = R.drawable.sharing_more;

        // We need to mock these two methods out so that they don't try to invoke
        // platform APIs - the stub object isn't complete enough.
        when(resolveInfo.loadLabel(any())).thenReturn(labelFromPackageName(packageName));
        when(resolveInfo.loadIcon(any())).thenReturn(null);

        return resolveInfo;
    }

    private List<ResolveInfo> createTestResolveInfos(List<String> packages) {
        List<ResolveInfo> infos = new ArrayList<>();
        for (String s : packages) {
            // Use the package name for the activity name as well, so each test
            // ResolveInfo ends up representing a package that has a single activity
            // with the same name as the package.
            infos.add(createStubResolveInfo(s));
        }
        return infos;
    }

    /**
     * Configure the sharesheet to use hardcoded layout constants so that the results of this test
     * don't depend on the screen size or DPI of the device it's being run on.
     */
    private void setUpLayoutConstants() {
        final int kTileWidth = 64;
        final int kTileMargin = 16;
        final int kTileVisualWidth = kTileWidth + 2 * kTileMargin;
        final int kScreenWidth = 4 * kTileVisualWidth + 2 * kTileMargin;

        ShareSheetUsageRankingHelper.FORCED_TILE_WIDTH_FOR_TEST = kTileWidth;
        ShareSheetUsageRankingHelper.FORCED_TILE_MARGIN_FOR_TEST = kTileMargin;
        ShareSheetUsageRankingHelper.FORCED_SCREEN_WIDTH_FOR_TEST = kScreenWidth;
    }

    @Before
    public void setUp() throws Exception {
        setUpLayoutConstants();

        ContextUtils.initApplicationContextForTests(
                new PackageManagerReplacingContext(ContextUtils.getApplicationContext(), this));

        MockitoAnnotations.initMocks(this);
        sActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfile = ProfileManager.getLastUsedRegularProfile();
                });
    }

    // Open the share sheet from the menu and wait for its open animation to
    // have completed.
    private void openShareSheet() {
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                sActivityTestRule.getActivity(),
                R.id.share_menu_id);

        BottomSheetController controller =
                sActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getBottomSheetController();
        BottomSheetTestSupport.waitForState(controller, BottomSheetController.SheetState.FULL);

        Assert.assertEquals(BottomSheetController.SheetState.FULL, controller.getSheetState());
    }

    // Replace the recent share history with the supplied map of usage counts.
    private void replaceRecentShareHistory(Map<String, Integer> recent) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ShareHistoryBridge.clear(mProfile);
                    for (Map.Entry<String, Integer> e : recent.entrySet()) {
                        for (int i = 0; i < e.getValue(); i++) {
                            ShareHistoryBridge.addShareEntry(mProfile, e.getKey());
                        }
                    }
                });
    }

    private void replaceAllShareHistory(Map<String, Integer> all) {
        // Not implemented yet. This method will require a new JNI interface
        // via ShareHistoryBridge, since there's currently no way to add
        // historical data, because production code never needs to do this.
        // TODO(crbug.com/40791331): Implement.
    }

    private void replaceStoredRanking(String type, List<String> apps) {
        // Not implemented yet. There's no JNI interface for replacing the stored
        // ranking, but there will be in the future.
        // TODO(crbug.com/40791331): Implement.
    }

    private void replaceSystemApps(List<String> apps) {
        mAvailableResolveInfos = createTestResolveInfos(apps);
    }

    public List<ResolveInfo> availableResolveInfos() {
        return mAvailableResolveInfos;
    }

    /**
     * This class wraps a Context, replacing its PackageManager with a shim PackageManager that
     * returns a configurable list of packages as available for ACTION_SEND type intents.
     */
    private class PackageManagerReplacingContext extends ContextWrapper {
        private ShareSheetTest mTest;

        public PackageManagerReplacingContext(Context baseContext, ShareSheetTest test) {
            super(baseContext);
            mTest = test;
        }

        @Override
        public PackageManager getPackageManager() {
            return new PackageManagerWrapper(super.getPackageManager()) {
                @Override
                public List<ResolveInfo> queryIntentActivities(Intent intent, int flags) {
                    if (intent.getAction().equals(Intent.ACTION_SEND)) {
                        return mTest.availableResolveInfos();
                    }
                    return PackageManagerReplacingContext.super
                            .getPackageManager()
                            .queryIntentActivities(intent, flags);
                }
            };
        }
    }

    /**
     * Returns the list of names of targets shown in the third party row of the sharing hub,
     * including targets that are off-screen.
     */
    private List<String> getShown3PTargets() {
        BottomSheetController controller =
                sActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getBottomSheetController();
        View sheetView = controller.getCurrentSheetContent().getContentView();
        View thirdPartyRow = sheetView.findViewById(R.id.share_sheet_other_apps);

        List<String> targets = new ArrayList<>();
        ViewGroup thirdPartyGroup = (ViewGroup) thirdPartyRow;
        for (int i = 0; i < thirdPartyGroup.getChildCount(); i++) {
            View v = thirdPartyGroup.getChildAt(i);
            View label = v.findViewById(R.id.text);
            targets.add(((TextView) label).getText().toString());
        }

        return targets;
    }

    /**
     * Creates a default test history, which is a map from history names (which are package + '/' +
     * activity) to usage counts. The generated map here is: { 'a': 10, 'b': 9, 'c': 8, ..., 'j': 1
     * }.
     */
    private Map<String, Integer> defaultTestHistory() {
        char suffix = 'a';
        int uses = 10;
        Map<String, Integer> history = new HashMap<>();
        while (uses > 0) {
            history.put(historyNameFromLabel("" + suffix), uses);
            suffix++;
            uses--;
        }
        return history;
    }

    /**
     * Creates a default set of available apps. This is a list of package names, which defaults to {
     * 'a', 'b', 'c', 'd', 'e' }.
     */
    private List<String> defaultTestSystemApps() {
        List<String> apps = new ArrayList<String>();
        for (String s : Arrays.asList("a", "b", "c", "d", "e")) {
            apps.add(packageNameFromLabel(s));
        }
        return apps;
    }

    @Test
    @SmallTest
    // 3P share sheet is not supported on auto.
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void nothingFromDefaultRankingAvailable() {
        replaceRecentShareHistory(defaultTestHistory());
        replaceSystemApps(defaultTestSystemApps());

        openShareSheet();

        // One might expect this to be:
        //   "a", "b", "c", "More"
        // but actually, what happens is that the initial ranking starts with:
        //   "A1", "A2", "A3", "More" (the actual apps in the default ranking)
        // and since La has more usage than those apps, all of which have
        // zero usage, La gets swapped in for the "lowest" app, which is A3.
        // After that, unavailable apps get erased, so we end up with:
        //   "", "", "a", "More"
        // and then empty slots are filled based on the ranking, yielding:
        //   "b", "c", "a", "More"
        Assert.assertEquals(Arrays.asList("b", "c", "a", "More"), getShown3PTargets());
    }
}
