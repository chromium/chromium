// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.GONE;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.isNotChecked;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.not;

import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.components.sync.PassphraseType;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/** Tests to make sure that PassphraseTypeDialogFragment presents the correct options. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PassphraseTypeDialogFragmentTest extends BlankUiTestActivityTestCase {
    private static final int RENDER_TEST_REVISION = 3;
    private static final String RENDER_TEST_DESCRIPTION =
            "Updated strings and re-ordering of the two options.";

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_TEST_REVISION)
                    .setDescription(RENDER_TEST_DESCRIPTION)
                    .setBugComponent(RenderTestRule.Component.SERVICES_SYNC)
                    .build();

    private static final String TAG = "PassphraseTypeDialogFragmentTest";

    private PassphraseTypeDialogFragment mTypeFragment;

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testKeystoreEncryptionOptions() {
        createFragment(PassphraseType.KEYSTORE_PASSPHRASE, true);
        onView(withId(R.id.explicit_passphrase_checkbox))
                .inRoot(isDialog())
                .check(matches(isNotChecked()));
        onView(withId(R.id.explicit_passphrase_checkbox)).check(matches(isEnabled()));
        onView(withId(R.id.keystore_passphrase_checkbox)).check(matches(isChecked()));
        onView(withId(R.id.keystore_passphrase_checkbox)).check(matches(isEnabled()));
        onView(withId(R.id.reset_sync_link)).check(matches(withEffectiveVisibility(GONE)));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testCustomEncryptionOptions() {
        createFragment(PassphraseType.CUSTOM_PASSPHRASE, true);
        onView(withId(R.id.explicit_passphrase_checkbox))
                .inRoot(isDialog())
                .check(matches(isChecked()));
        onView(withId(R.id.explicit_passphrase_checkbox)).check(matches(not(isEnabled())));
        onView(withId(R.id.keystore_passphrase_checkbox)).check(matches(isNotChecked()));
        onView(withId(R.id.keystore_passphrase_checkbox)).check(matches(not(isEnabled())));
        onView(withId(R.id.reset_sync_link)).check(matches(withEffectiveVisibility(VISIBLE)));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testFrozenImplicitEncryptionOptions() {
        createFragment(PassphraseType.FROZEN_IMPLICIT_PASSPHRASE, true);
        onView(withId(R.id.explicit_passphrase_checkbox))
                .inRoot(isDialog())
                .check(matches(isChecked()));
        onView(withId(R.id.explicit_passphrase_checkbox)).check(matches(not(isEnabled())));
        onView(withId(R.id.keystore_passphrase_checkbox)).check(matches(isNotChecked()));
        onView(withId(R.id.keystore_passphrase_checkbox)).check(matches(not(isEnabled())));
        onView(withId(R.id.reset_sync_link)).check(matches(withEffectiveVisibility(VISIBLE)));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testImplicitEncryptionOptions() {
        createFragment(PassphraseType.IMPLICIT_PASSPHRASE, true);
        onView(withId(R.id.explicit_passphrase_checkbox))
                .inRoot(isDialog())
                .check(matches(isNotChecked()));
        onView(withId(R.id.explicit_passphrase_checkbox)).check(matches(isEnabled()));
        onView(withId(R.id.keystore_passphrase_checkbox)).check(matches(isChecked()));
        onView(withId(R.id.keystore_passphrase_checkbox)).check(matches(isEnabled()));
        onView(withId(R.id.reset_sync_link)).check(matches(withEffectiveVisibility(GONE)));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testKeystoreEncryptionOptionsCustomPassphraseDisallowed() {
        createFragment(PassphraseType.KEYSTORE_PASSPHRASE, false);
        onView(withId(R.id.explicit_passphrase_checkbox))
                .inRoot(isDialog())
                .check(matches(isNotChecked()));
        onView(withId(R.id.explicit_passphrase_checkbox)).check(matches(not(isEnabled())));
        onView(withId(R.id.keystore_passphrase_checkbox)).check(matches(isChecked()));
        onView(withId(R.id.keystore_passphrase_checkbox)).check(matches(isEnabled()));
        onView(withId(R.id.reset_sync_link)).check(matches(withEffectiveVisibility(GONE)));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testImplicitEncryptionOptionsCustomPassphraseDisallowed() {
        createFragment(PassphraseType.IMPLICIT_PASSPHRASE, false);
        onView(withId(R.id.explicit_passphrase_checkbox))
                .inRoot(isDialog())
                .check(matches(isNotChecked()));
        onView(withId(R.id.explicit_passphrase_checkbox)).check(matches(not(isEnabled())));
        onView(withId(R.id.keystore_passphrase_checkbox)).check(matches(isChecked()));
        onView(withId(R.id.keystore_passphrase_checkbox)).check(matches(isEnabled()));
        onView(withId(R.id.reset_sync_link)).check(matches(withEffectiveVisibility(GONE)));
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "Sync"})
    public void testKeystorePassphraseRendering() throws IOException {
        createFragment(PassphraseType.KEYSTORE_PASSPHRASE, true);
        mRenderTestRule.render(getDialogView(), "keystore_passphrase");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "Sync"})
    public void testCustomPassphraseRendering() throws IOException {
        createFragment(PassphraseType.CUSTOM_PASSPHRASE, true);
        mRenderTestRule.render(getDialogView(), "custom_passphrase");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "Sync"})
    public void testFrozenImplicitPassphraseRendering() throws IOException {
        createFragment(PassphraseType.FROZEN_IMPLICIT_PASSPHRASE, true);
        mRenderTestRule.render(getDialogView(), "frozen_implicit_passphrase");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "Sync"})
    public void testImplicitPassphraseRendering() throws IOException {
        createFragment(PassphraseType.IMPLICIT_PASSPHRASE, true);
        mRenderTestRule.render(getDialogView(), "implicit_passphrase");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "Sync"})
    public void testKeystorePassphraseWithCustomPassphraseDisallowedRendering() throws IOException {
        createFragment(PassphraseType.KEYSTORE_PASSPHRASE, false);
        mRenderTestRule.render(
                getDialogView(), "keystore_passphrase_with_custom_passphrase_disallowed");
    }

    public void createFragment(@PassphraseType int type, boolean isCustomPassphraseAllowed) {
        mTypeFragment = PassphraseTypeDialogFragment.create(type, isCustomPassphraseAllowed);
        mTypeFragment.show(getActivity().getSupportFragmentManager(), TAG);
        ActivityTestUtils.waitForFragment(getActivity(), TAG);
    }

    private View getDialogView() {
        return mTypeFragment.getDialog().getWindow().getDecorView();
    }
}
