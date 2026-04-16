// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui.batch_upload_card;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;

import android.app.Activity;

import androidx.lifecycle.LifecycleOwner;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;

import org.chromium.base.Callback;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.ui.batch_upload_card.BatchUploadCardCoordinator.EntryPoint;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.LocalDataDescription;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.MockitoHelper;

import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.Set;

/**
 * Unit tests for {@link BatchUploadCardMediator}.
 *
 * <p>TODO(crbug.com/493130564): Revert to regular runner after
 * MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS launch.
 */
@RunWith(ParameterizedRobolectricTestRunner.class)
public class BatchUploadCardMediatorTest {
    @Rule(order = Rule.DEFAULT_ORDER - 1)
    public final BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    @Parameters(name = "{index}_isIdentityMgr={0}")
    public static Collection parameters() {
        return Arrays.asList(false, true);
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Profile mProfile;
    @Mock private PropertyModel mModel;
    @Mock private OneshotSupplierImpl<SnackbarManager> mSnackbarManager;
    @Mock private ReauthenticatorBridge mReauthenticatorMock;
    @Mock private SyncService mSyncService;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;

    private Activity mActivity;
    private BatchUploadCardMediator mMediator;
    private final boolean mIsIdentityManagerSourceOfAccounts;

    public BatchUploadCardMediatorTest(boolean isIdentityManagerSourceOfAccounts) {
        mIsIdentityManagerSourceOfAccounts = isIdentityManagerSourceOfAccounts;
    }

    @Before
    public void setUp() {
        FeatureOverrides.overrideFlag(
                SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS,
                mIsIdentityManagerSourceOfAccounts);
        // Setup service mocks.
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        ReauthenticatorBridge.setInstanceForTesting(mReauthenticatorMock);
        doReturn(mProfile).when(mProfile).getOriginalProfile();
        doReturn(mIdentityManager).when(mIdentityServicesProvider).getIdentityManager(any());
    }

    @Test
    public void testBookmarkBatchUploadCardDoesNotPresentWhenNoLocalDataExist() {
        doAnswer(
                        args -> {
                            HashMap<Integer, LocalDataDescription> localDataDescription =
                                    new HashMap<>();
                            localDataDescription.put(
                                    DataType.PASSWORDS,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            localDataDescription.put(
                                    DataType.BOOKMARKS,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            localDataDescription.put(
                                    DataType.READING_LIST,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            args.getArgument(1, Callback.class).onResult(localDataDescription);
                            return null;
                        })
                .when(mSyncService)
                .getLocalDataDescriptions(
                        eq(Set.of(DataType.BOOKMARKS, DataType.PASSWORDS, DataType.READING_LIST)),
                        MockitoHelper.anyCallback());

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (activity) -> {
                            mMediator =
                                    new BatchUploadCardMediator(
                                            activity,
                                            (LifecycleOwner) activity,
                                            mModalDialogManager,
                                            mProfile,
                                            mModel,
                                            mSnackbarManager,
                                            () -> {},
                                            EntryPoint.BOOKMARK_MANAGER);
                        });
        Assert.assertFalse(mMediator.shouldBeVisible());
    }

    @Test
    public void testBookmarkBatchUploadCardDoesNotPresentWhenOnlyLocalPasswordsExist() {
        doAnswer(
                        args -> {
                            HashMap<Integer, LocalDataDescription> localDataDescription =
                                    new HashMap<>();
                            localDataDescription.put(
                                    DataType.PASSWORDS,
                                    new LocalDataDescription(1, new String[] {"example.com"}, 1));
                            localDataDescription.put(
                                    DataType.BOOKMARKS,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            localDataDescription.put(
                                    DataType.READING_LIST,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            args.getArgument(1, Callback.class).onResult(localDataDescription);
                            return null;
                        })
                .when(mSyncService)
                .getLocalDataDescriptions(
                        eq(Set.of(DataType.BOOKMARKS, DataType.PASSWORDS, DataType.READING_LIST)),
                        MockitoHelper.anyCallback());

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (activity) -> {
                            mMediator =
                                    new BatchUploadCardMediator(
                                            activity,
                                            (LifecycleOwner) activity,
                                            mModalDialogManager,
                                            mProfile,
                                            mModel,
                                            mSnackbarManager,
                                            () -> {},
                                            EntryPoint.BOOKMARK_MANAGER);
                        });
        Assert.assertFalse(mMediator.shouldBeVisible());
    }

    @Test
    public void testBookmarkBatchUploadCardPresentWhenOnlyLocalBookmarksExist() {
        doAnswer(
                        args -> {
                            HashMap<Integer, LocalDataDescription> localDataDescription =
                                    new HashMap<>();
                            localDataDescription.put(
                                    DataType.PASSWORDS,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            localDataDescription.put(
                                    DataType.BOOKMARKS,
                                    new LocalDataDescription(1, new String[] {"example.com"}, 1));
                            localDataDescription.put(
                                    DataType.READING_LIST,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            args.getArgument(1, Callback.class).onResult(localDataDescription);
                            return null;
                        })
                .when(mSyncService)
                .getLocalDataDescriptions(
                        eq(Set.of(DataType.BOOKMARKS, DataType.PASSWORDS, DataType.READING_LIST)),
                        MockitoHelper.anyCallback());

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (activity) -> {
                            mMediator =
                                    new BatchUploadCardMediator(
                                            activity,
                                            (LifecycleOwner) activity,
                                            mModalDialogManager,
                                            mProfile,
                                            mModel,
                                            mSnackbarManager,
                                            () -> {},
                                            EntryPoint.BOOKMARK_MANAGER);
                        });
        Assert.assertTrue(mMediator.shouldBeVisible());
    }

    @Test
    public void testBookmarkBatchUploadCardPresentWhenOnlyLocalReadingListEntriesExist() {
        doAnswer(
                        args -> {
                            HashMap<Integer, LocalDataDescription> localDataDescription =
                                    new HashMap<>();
                            localDataDescription.put(
                                    DataType.PASSWORDS,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            localDataDescription.put(
                                    DataType.BOOKMARKS,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            localDataDescription.put(
                                    DataType.READING_LIST,
                                    new LocalDataDescription(1, new String[] {"example.com"}, 1));
                            args.getArgument(1, Callback.class).onResult(localDataDescription);
                            return null;
                        })
                .when(mSyncService)
                .getLocalDataDescriptions(
                        eq(Set.of(DataType.BOOKMARKS, DataType.PASSWORDS, DataType.READING_LIST)),
                        MockitoHelper.anyCallback());

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (activity) -> {
                            mMediator =
                                    new BatchUploadCardMediator(
                                            activity,
                                            (LifecycleOwner) activity,
                                            mModalDialogManager,
                                            mProfile,
                                            mModel,
                                            mSnackbarManager,
                                            () -> {},
                                            EntryPoint.BOOKMARK_MANAGER);
                        });
        Assert.assertTrue(mMediator.shouldBeVisible());
    }

    @Test
    public void testSettingsBatchUploadCardDoesNotPresentWhenNoLocalDataExist() {
        doAnswer(
                        args -> {
                            HashMap<Integer, LocalDataDescription> localDataDescription =
                                    new HashMap<>();
                            localDataDescription.put(
                                    DataType.PASSWORDS,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            localDataDescription.put(
                                    DataType.BOOKMARKS,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            localDataDescription.put(
                                    DataType.READING_LIST,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            args.getArgument(1, Callback.class).onResult(localDataDescription);
                            return null;
                        })
                .when(mSyncService)
                .getLocalDataDescriptions(
                        eq(Set.of(DataType.BOOKMARKS, DataType.PASSWORDS, DataType.READING_LIST)),
                        MockitoHelper.anyCallback());

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (activity) -> {
                            mMediator =
                                    new BatchUploadCardMediator(
                                            activity,
                                            (LifecycleOwner) activity,
                                            mModalDialogManager,
                                            mProfile,
                                            mModel,
                                            mSnackbarManager,
                                            () -> {},
                                            EntryPoint.SETTINGS);
                        });
        Assert.assertFalse(mMediator.shouldBeVisible());
    }

    @Test
    public void testSettingsBatchUploadCardPresentWhenOnlyLocalPasswordsExists() {
        doAnswer(
                        args -> {
                            HashMap<Integer, LocalDataDescription> localDataDescription =
                                    new HashMap<>();
                            localDataDescription.put(
                                    DataType.PASSWORDS,
                                    new LocalDataDescription(1, new String[] {"example.com"}, 1));
                            localDataDescription.put(
                                    DataType.BOOKMARKS,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            localDataDescription.put(
                                    DataType.READING_LIST,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            args.getArgument(1, Callback.class).onResult(localDataDescription);
                            return null;
                        })
                .when(mSyncService)
                .getLocalDataDescriptions(
                        eq(Set.of(DataType.BOOKMARKS, DataType.PASSWORDS, DataType.READING_LIST)),
                        MockitoHelper.anyCallback());

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (activity) -> {
                            mMediator =
                                    new BatchUploadCardMediator(
                                            activity,
                                            (LifecycleOwner) activity,
                                            mModalDialogManager,
                                            mProfile,
                                            mModel,
                                            mSnackbarManager,
                                            () -> {},
                                            EntryPoint.SETTINGS);
                        });
        Assert.assertTrue(mMediator.shouldBeVisible());
    }

    @Test
    public void testSettingsBatchUploadCardPresentWhenOnlyLocalBookmarksExist() {
        doAnswer(
                        args -> {
                            HashMap<Integer, LocalDataDescription> localDataDescription =
                                    new HashMap<>();
                            localDataDescription.put(
                                    DataType.PASSWORDS,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            localDataDescription.put(
                                    DataType.BOOKMARKS,
                                    new LocalDataDescription(1, new String[] {"example.com"}, 1));
                            localDataDescription.put(
                                    DataType.READING_LIST,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            args.getArgument(1, Callback.class).onResult(localDataDescription);
                            return null;
                        })
                .when(mSyncService)
                .getLocalDataDescriptions(
                        eq(Set.of(DataType.BOOKMARKS, DataType.PASSWORDS, DataType.READING_LIST)),
                        MockitoHelper.anyCallback());

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (activity) -> {
                            mMediator =
                                    new BatchUploadCardMediator(
                                            activity,
                                            (LifecycleOwner) activity,
                                            mModalDialogManager,
                                            mProfile,
                                            mModel,
                                            mSnackbarManager,
                                            () -> {},
                                            EntryPoint.SETTINGS);
                        });
        Assert.assertTrue(mMediator.shouldBeVisible());
    }

    @Test
    public void testSettingsBatchUploadCardPresentWhenOnlyLocalReadingListEntriesExist() {
        doAnswer(
                        args -> {
                            HashMap<Integer, LocalDataDescription> localDataDescription =
                                    new HashMap<>();
                            localDataDescription.put(
                                    DataType.PASSWORDS,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            localDataDescription.put(
                                    DataType.BOOKMARKS,
                                    new LocalDataDescription(0, new String[] {}, 0));
                            localDataDescription.put(
                                    DataType.READING_LIST,
                                    new LocalDataDescription(1, new String[] {"example.com"}, 1));
                            args.getArgument(1, Callback.class).onResult(localDataDescription);
                            return null;
                        })
                .when(mSyncService)
                .getLocalDataDescriptions(
                        eq(Set.of(DataType.BOOKMARKS, DataType.PASSWORDS, DataType.READING_LIST)),
                        MockitoHelper.anyCallback());

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (activity) -> {
                            mMediator =
                                    new BatchUploadCardMediator(
                                            activity,
                                            (LifecycleOwner) activity,
                                            mModalDialogManager,
                                            mProfile,
                                            mModel,
                                            mSnackbarManager,
                                            () -> {},
                                            EntryPoint.SETTINGS);
                        });
        Assert.assertTrue(mMediator.shouldBeVisible());
    }
}
