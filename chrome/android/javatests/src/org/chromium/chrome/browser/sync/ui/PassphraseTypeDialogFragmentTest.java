// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import android.support.test.InstrumentationRegistry;
import android.widget.CheckedTextView;
import android.widget.HeaderViewListAdapter;
import android.widget.ListView;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.sync.PassphraseType;

/**
 * Tests to make sure that PassphraseTypeDialogFragment presents the correct options.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PassphraseTypeDialogFragmentTest {
    @Rule
    public SyncTestRule mSyncTestRule = new SyncTestRule();

    private static final String TAG = "PassphraseTypeDialogFragmentTest";

    private static final boolean ENABLED = true;
    private static final boolean DISABLED = false;
    private static final boolean CHECKED = true;
    private static final boolean UNCHECKED = false;

    private static class TypeOptions {
        public final @PassphraseType int type;
        public final boolean isEnabled;
        public final boolean isChecked;
        public TypeOptions(@PassphraseType int type, boolean isEnabled, boolean isChecked) {
            this.type = type;
            this.isEnabled = isEnabled;
            this.isChecked = isChecked;
        }
    }

    private PassphraseTypeDialogFragment mTypeFragment;

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testKeystoreEncryptionOptions() {
        createFragment(PassphraseType.KEYSTORE_PASSPHRASE, true);
        assertPassphraseTypeOptions(false,
                new TypeOptions(PassphraseType.CUSTOM_PASSPHRASE, ENABLED, UNCHECKED),
                new TypeOptions(PassphraseType.KEYSTORE_PASSPHRASE, ENABLED, CHECKED));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testCustomEncryptionOptions() {
        createFragment(PassphraseType.CUSTOM_PASSPHRASE, true);
        assertPassphraseTypeOptions(true,
                new TypeOptions(PassphraseType.CUSTOM_PASSPHRASE, DISABLED, CHECKED),
                new TypeOptions(PassphraseType.KEYSTORE_PASSPHRASE, DISABLED, UNCHECKED));
    }

    /*
     * @SmallTest
     * @Feature({"Sync"})
     */
    @Test
    @FlakyTest(message = "crbug.com/588050")
    public void testFrozenImplicitEncryptionOptions() {
        createFragment(PassphraseType.FROZEN_IMPLICIT_PASSPHRASE, true);
        assertPassphraseTypeOptions(true,
                new TypeOptions(PassphraseType.FROZEN_IMPLICIT_PASSPHRASE, DISABLED, CHECKED),
                new TypeOptions(PassphraseType.KEYSTORE_PASSPHRASE, DISABLED, UNCHECKED));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testImplicitEncryptionOptions() {
        createFragment(PassphraseType.IMPLICIT_PASSPHRASE, true);
        assertPassphraseTypeOptions(false,
                new TypeOptions(PassphraseType.CUSTOM_PASSPHRASE, ENABLED, UNCHECKED),
                new TypeOptions(PassphraseType.IMPLICIT_PASSPHRASE, ENABLED, CHECKED));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testKeystoreEncryptionOptionsEncryptEverythingDisallowed() {
        createFragment(PassphraseType.KEYSTORE_PASSPHRASE, false);
        assertPassphraseTypeOptions(false,
                new TypeOptions(PassphraseType.CUSTOM_PASSPHRASE, DISABLED, UNCHECKED),
                new TypeOptions(PassphraseType.KEYSTORE_PASSPHRASE, ENABLED, CHECKED));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testImplicitEncryptionOptionsEncryptEverythingDisallowed() {
        createFragment(PassphraseType.IMPLICIT_PASSPHRASE, false);
        assertPassphraseTypeOptions(false,
                new TypeOptions(PassphraseType.CUSTOM_PASSPHRASE, DISABLED, UNCHECKED),
                new TypeOptions(PassphraseType.IMPLICIT_PASSPHRASE, ENABLED, CHECKED));
    }

    public void createFragment(@PassphraseType int type, boolean isEncryptEverythingAllowed) {
        mTypeFragment = PassphraseTypeDialogFragment.create(type, 0, isEncryptEverythingAllowed);
        mTypeFragment.show(mSyncTestRule.getActivity().getSupportFragmentManager(), TAG);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    public void assertPassphraseTypeOptions(boolean hasFooter, TypeOptions... optionsList) {
        ListView listView =
                (ListView) mTypeFragment.getDialog().findViewById(R.id.passphrase_type_list);
        PassphraseTypeDialogFragment.Adapter adapter;
        if (hasFooter) {
            HeaderViewListAdapter headerAdapter = (HeaderViewListAdapter) listView.getAdapter();
            adapter = (PassphraseTypeDialogFragment.Adapter) headerAdapter.getWrappedAdapter();
        } else {
            adapter = (PassphraseTypeDialogFragment.Adapter) listView.getAdapter();
        }

        Assert.assertEquals(
                "Number of options doesn't match.", optionsList.length, adapter.getCount());
        for (int i = 0; i < optionsList.length; i++) {
            TypeOptions options = optionsList[i];
            Assert.assertEquals(
                    "Option " + i + " type is wrong.", options.type, adapter.getType(i));
            CheckedTextView checkedView = (CheckedTextView) listView.getChildAt(i);
            Assert.assertEquals("Option " + i + " enabled state is wrong.", options.isEnabled,
                    checkedView.isEnabled());
            Assert.assertEquals("Option " + i + " checked state is wrong.", options.isChecked,
                    checkedView.isChecked());
        }
    }
}
