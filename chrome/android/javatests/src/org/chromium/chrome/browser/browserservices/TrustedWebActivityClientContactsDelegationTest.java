// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.net.Uri;
import android.os.Bundle;
import android.os.RemoteException;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.browser.trusted.TrustedWebActivityCallback;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.IntentUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.browserservices.permissiondelegation.InstalledWebappPermissionManager;
import org.chromium.components.embedder_support.util.Origin;

import java.util.Set;
import java.util.concurrent.TimeoutException;

/**
 * Tests TrustedWebActivityClient contact picker delegation.
 *
 * <p>See {@link TrustedWebActivityClientLocationDelegationTest} for the detailed test strategy.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class TrustedWebActivityClientContactsDelegationTest {
    private static final Uri SCOPE = Uri.parse("https://www.example.com/contactpicker");
    private static final Origin ORIGIN = Origin.create(SCOPE);

    private static final String TEST_SUPPORT_PACKAGE = "org.chromium.chrome.tests.support";

    private static final String COMMAND_FETCH_CONTACTS = "fetchContacts";
    private static final String COMMAND_FETCH_CONTACT_ICON = "fetchContactIcon";
    private static final String EXTRA_ON_CONTACT_FETCH_ERROR = "onContactFetchError";

    private TrustedWebActivityClient mClient;

    @Before
    public void setUp() throws TimeoutException, RemoteException {
        mClient = TrustedWebActivityClient.getInstance();

        // TestTrustedWebActivityService is in the test support apk.
        InstalledWebappPermissionManager.addDelegateApp(ORIGIN, TEST_SUPPORT_PACKAGE);
    }

    /** Tests {@link TrustedWebActivityClient#checkContactPermission} */
    @Test
    @SmallTest
    public void testCheckContactPermission() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();

        TrustedWebActivityClient.PermissionCallback callback =
                (app, settingValue) -> callbackHelper.notifyCalled();

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> mClient.checkContactPermission(SCOPE.toString(), callback));
        callbackHelper.waitForOnly();
    }

    /** Tests {@link TrustedWebActivityClient#fetchContacts} */
    @Test
    @SmallTest
    public void testFetchContacts() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();

        TrustedWebActivityCallback fetchContactsCallback =
                new TrustedWebActivityCallback() {
                    @Override
                    public void onExtraCallback(String callbackName, @Nullable Bundle bundle) {
                        if (TextUtils.equals(callbackName, COMMAND_FETCH_CONTACTS)) {
                            Assert.assertNotNull(bundle);
                            Assert.assertTrue(bundle.containsKey("contacts"));
                            Bundle contact =
                                    IntentUtils.<Bundle>safeGetParcelableArrayList(
                                                    bundle, "contacts")
                                            .get(0);
                            Assert.assertEquals(
                                    contact.keySet(),
                                    Set.of("id", "name", "email", "tel", "address"));

                            callbackHelper.notifyCalled();
                        }
                    }
                };
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () ->
                        mClient.fetchContacts(
                                SCOPE.toString(), true, true, true, true, fetchContactsCallback));
        callbackHelper.waitForOnly();
    }

    /** Tests {@link TrustedWebActivityClient#fetchContacts} */
    @Test
    @SmallTest
    public void testFetchContactsNoConnection() throws TimeoutException {
        Origin otherOrigin = Origin.createOrThrow("https://www.websitewithouttwa.com/");
        CallbackHelper callbackHelper = new CallbackHelper();

        TrustedWebActivityCallback fetchContactsCallback =
                new TrustedWebActivityCallback() {
                    @Override
                    public void onExtraCallback(String callbackName, @Nullable Bundle bundle) {
                        if (TextUtils.equals(callbackName, EXTRA_ON_CONTACT_FETCH_ERROR)) {
                            Assert.assertNotNull(bundle);
                            String message = bundle.getString("message");
                            Assert.assertEquals("NoTwaFound", message);
                            callbackHelper.notifyCalled();
                        }
                    }
                };
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () ->
                        mClient.fetchContacts(
                                otherOrigin.toString(),
                                true,
                                true,
                                true,
                                true,
                                fetchContactsCallback));
        callbackHelper.waitForOnly();
    }

    /** Tests {@link TrustedWebActivityClient#fetchContactIcon} */
    @Test
    @SmallTest
    public void testFetchContactIcon() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();

        TrustedWebActivityCallback fetchContactsCallback =
                new TrustedWebActivityCallback() {
                    @Override
                    public void onExtraCallback(String callbackName, @Nullable Bundle bundle) {
                        if (TextUtils.equals(callbackName, COMMAND_FETCH_CONTACT_ICON)) {
                            Assert.assertNotNull(bundle);
                            Assert.assertTrue(bundle.containsKey("icon"));
                            callbackHelper.notifyCalled();
                        }
                    }
                };
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () ->
                        mClient.fetchContactIcon(
                                SCOPE.toString(), "id123", 16, fetchContactsCallback));
        callbackHelper.waitForOnly();
    }

    /** Tests {@link TrustedWebActivityClient#fetchContactIcon} */
    @Test
    @SmallTest
    public void testFetchContactIconNoConnection() throws TimeoutException {
        Origin otherOrigin = Origin.createOrThrow("https://www.websitewithouttwa.com/");
        CallbackHelper callbackHelper = new CallbackHelper();

        TrustedWebActivityCallback fetchContactsCallback =
                new TrustedWebActivityCallback() {
                    @Override
                    public void onExtraCallback(String callbackName, @Nullable Bundle bundle) {
                        if (TextUtils.equals(callbackName, EXTRA_ON_CONTACT_FETCH_ERROR)) {
                            Assert.assertNotNull(bundle);
                            String message = bundle.getString("message");
                            Assert.assertEquals("NoTwaFound", message);
                            callbackHelper.notifyCalled();
                        }
                    }
                };
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () ->
                        mClient.fetchContactIcon(
                                otherOrigin.toString(), "id123", 16, fetchContactsCallback));
        callbackHelper.waitForOnly();
    }
}
