// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;

/**
 * Unit tests for {@link TabModelSelectorProfileSupplierTest}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabModelSelectorProfileSupplierTest {
    @Mock
    Profile mProfile;
    @Mock
    Profile mIncognitoProfile;
    @Mock
    Callback<Profile> mProfileCallback1;
    @Mock
    Callback<Profile> mProfileCallback2;

    ObservableSupplierImpl<TabModelSelector> mTabModelSelectorSupplier =
            new ObservableSupplierImpl<>();

    TabModelSelectorProfileSupplier mSupplier;
    MockTabModelSelector mSelector;
    MockTabModel mNormalModel;
    MockTabModel mIncognitoModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        initTabModelSelector();
        mSupplier = new TabModelSelectorProfileSupplier(mTabModelSelectorSupplier);
        doReturn(true).when(mIncognitoProfile).isOffTheRecord();
    }

    private void initTabModelSelector() {
        mSelector = new MockTabModelSelector(0, 0, null);
        mNormalModel = new MockTabModel(false, null) {
            @Override
            public Profile getProfile() {
                return mProfile;
            };
        };
        mIncognitoModel = new MockTabModel(true, null) {
            @Override
            public Profile getProfile() {
                return mIncognitoProfile;
            }
        };
    }

    @Test
    public void testInitialTabModelHasNoProfile_initializedLater() {
        mTabModelSelectorSupplier.set(mSelector);
        Profile profile = mProfile;
        mProfile = null;
        mSelector.initializeTabModels(mNormalModel, mIncognitoModel);
        Assert.assertFalse(mSupplier.hasValue());

        mProfile = profile;
        mSelector.markTabStateInitialized();
        Assert.assertEquals(mProfile, mSupplier.get());
    }

    @Test
    public void testObserversFired() {
        mSupplier.addObserver(mProfileCallback1);
        mSupplier.addObserver(mProfileCallback2);

        mTabModelSelectorSupplier.set(mSelector);
        Assert.assertFalse(mSupplier.hasValue());
        mSelector.initializeTabModels(mNormalModel, mIncognitoModel);
        mSelector.markTabStateInitialized();

        Assert.assertEquals(mProfile, mSupplier.get());
        verify(mProfileCallback1, times(1)).onResult(mProfile);
        verify(mProfileCallback2, times(1)).onResult(mProfile);

        mSelector.selectModel(true);
        Assert.assertEquals(mIncognitoProfile, mSupplier.get());
        verify(mProfileCallback1, times(1)).onResult(mIncognitoProfile);
        verify(mProfileCallback2, times(1)).onResult(mIncognitoProfile);
    }

    @Test
    public void tesOTRProfileReturnsForIncognitoTabModel() {
        mTabModelSelectorSupplier.set(mSelector);
        mSelector.initializeTabModels(mNormalModel, mIncognitoModel);
        mSelector.markTabStateInitialized();
        mSelector.selectModel(true);

        Assert.assertEquals(mIncognitoProfile, mSupplier.get());
    }

    @Test
    public void tesRegularProfileReturnsForRegularTabModel() {
        mTabModelSelectorSupplier.set(mSelector);
        mSelector.initializeTabModels(mNormalModel, mIncognitoModel);
        mSelector.markTabStateInitialized();
        Assert.assertEquals(mProfile, mSupplier.get());

        mSelector.selectModel(true);
        Assert.assertEquals(mIncognitoProfile, mSupplier.get());

        mSelector.selectModel(false);
        Assert.assertEquals(mProfile, mSupplier.get());
    }

    @Test
    public void testDestroyPreInitialization() {
        mSupplier.destroy();

        mTabModelSelectorSupplier.set(mSelector);
        mSelector.initializeTabModels(mNormalModel, mIncognitoModel);
        mSelector.markTabStateInitialized();
        Assert.assertFalse(mSupplier.hasValue());
    }

    @Test
    public void testDestroyPostInitialization() {
        mTabModelSelectorSupplier.set(mSelector);
        mSupplier.destroy();

        mSelector.initializeTabModels(mNormalModel, mIncognitoModel);
        mSelector.markTabStateInitialized();
        Assert.assertFalse(mSupplier.hasValue());
    }

    @Test
    public void testPreviouslyInitializedSelector() {
        mSelector.initializeTabModels(mNormalModel, mIncognitoModel);
        mSelector.markTabStateInitialized();

        mTabModelSelectorSupplier.set(mSelector);

        Assert.assertEquals(mProfile, mSupplier.get());
    }

    @Test
    public void testSwapTabModelSelector() {
        mTabModelSelectorSupplier.set(mSelector);
        mSelector.initializeTabModels(mNormalModel, mIncognitoModel);
        mSelector.markTabStateInitialized();
        Assert.assertEquals(mProfile, mSupplier.get());

        Profile profile2 = mock(Profile.class);
        Profile incognitoProfile2 = mock(Profile.class);
        MockTabModelSelector selector2 = new MockTabModelSelector(0, 0, null);
        MockTabModel normalModel2 = new MockTabModel(false, null) {
            @Override
            public Profile getProfile() {
                return profile2;
            };
        };
        MockTabModel incognitoModel2 = new MockTabModel(true, null) {
            @Override
            public Profile getProfile() {
                return incognitoProfile2;
            }
        };
        selector2.initializeTabModels(normalModel2, incognitoModel2);
        selector2.markTabStateInitialized();
        mTabModelSelectorSupplier.set(selector2);

        Assert.assertEquals(profile2, mSupplier.get());

        // Change the model on the no longer registered selector and ensure the profile does not
        // change.
        mSelector.selectModel(true);
        Assert.assertEquals(profile2, mSupplier.get());
        mSelector.selectModel(false);
        Assert.assertEquals(profile2, mSupplier.get());

        // Change the model on the registered selector and ensure the profile changes.
        selector2.selectModel(true);
        Assert.assertEquals(incognitoProfile2, mSupplier.get());
    }
}
