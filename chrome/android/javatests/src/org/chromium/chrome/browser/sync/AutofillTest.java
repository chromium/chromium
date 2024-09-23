// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.util.Pair;

import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.sync.protocol.AutofillProfileSpecifics;
import org.chromium.components.sync.protocol.EntitySpecifics;

import java.util.ArrayList;
import java.util.List;

/** Test suite for the autofill profile sync data type. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillTest {
    @Rule public SyncTestRule mSyncTestRule = new SyncTestRule();

    private static final String AUTOFILL_TYPE = "Autofill Profiles";

    private static final String GUID = "EDC609ED-7EEE-4F27-B00C-423242A9C44B";

    private static final String STREET = "1600 Amphitheatre Pkwy";
    private static final String CITY = "Mountain View";
    private static final String MODIFIED_CITY = "Sunnyvale";
    private static final String STATE = "CA";
    private static final String ZIP = "94043";

    // A container to store autofill profile information for data verification.
    private static class Autofill {
        public final String id;
        public final String clientTagHash;
        public final String street;
        public final String city;
        public final String state;
        public final String zip;

        public Autofill(
                String id,
                String clientTagHash,
                String street,
                String city,
                String state,
                String zip) {
            this.id = id;
            this.clientTagHash = clientTagHash;
            this.street = street;
            this.city = city;
            this.state = state;
            this.zip = zip;
        }
    }

    @Before
    public void setUp() throws Exception {
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        // Make sure the initial state is clean.
        assertClientAutofillProfileCount(0);
        assertServerAutofillProfileCountWithName(0, STREET);
    }

    // Test syncing an autofill profile from server to client.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadAutofill() throws Exception {
        addServerAutofillProfile(getServerAutofillProfile(STREET, CITY, STATE, ZIP));
        SyncTestUtil.triggerSync();
        waitForClientAutofillProfileCount(1);

        // Verify data synced to client.
        List<Autofill> autofills = getClientAutofillProfiles();
        Assert.assertEquals(
                "Only the injected autofill should exist on the client.", 1, autofills.size());
        Autofill autofill = autofills.get(0);
        Assert.assertEquals("The wrong street was found for the address.", STREET, autofill.street);
        Assert.assertEquals("The wrong city was found for the autofill.", CITY, autofill.city);
        Assert.assertEquals("The wrong state was found for the autofill.", STATE, autofill.state);
        Assert.assertEquals("The wrong zip was found for the autofill.", ZIP, autofill.zip);
    }

    // Test syncing an autofill profile modification from server to client.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadAutofillModification() throws Exception {
        // Add the entity to test modifying.
        addServerAutofillProfile(getServerAutofillProfile(STREET, CITY, STATE, ZIP));
        SyncTestUtil.triggerSync();
        waitForClientAutofillProfileCount(1);

        // Modify on server, sync, and verify modification locally.
        Autofill autofill = getClientAutofillProfiles().get(0);
        mSyncTestRule
                .getFakeServerHelper()
                .modifyEntitySpecifics(
                        autofill.id, getServerAutofillProfile(STREET, MODIFIED_CITY, STATE, ZIP));
        SyncTestUtil.triggerSync();
        mSyncTestRule.pollInstrumentationThread(
                () -> {
                    try {
                        Autofill modifiedAutofill = getClientAutofillProfiles().get(0);
                        Criteria.checkThat(modifiedAutofill.city, Matchers.is(MODIFIED_CITY));
                    } catch (JSONException ex) {
                        throw new RuntimeException(ex);
                    }
                });
    }

    // Test syncing an autofill profile deletion from server to client.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadDeletedAutofill() throws Exception {
        // Add the entity to test deleting.
        addServerAutofillProfile(getServerAutofillProfile(STREET, CITY, STATE, ZIP));
        SyncTestUtil.triggerSync();
        waitForClientAutofillProfileCount(1);

        // Delete on server, sync, and verify deleted locally.
        Autofill autofill = getClientAutofillProfiles().get(0);
        mSyncTestRule.getFakeServerHelper().deleteEntity(autofill.id, autofill.clientTagHash);
        SyncTestUtil.triggerSync();
        waitForClientAutofillProfileCount(0);
    }

    // Test that autofill profiles don't get synced if the data type is disabled.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testDisabledNoDownloadAutofill() throws Exception {
        // The AUTOFILL type here controls both AUTOFILL and AUTOFILL_PROFILE.
        mSyncTestRule.disableDataType(UserSelectableType.AUTOFILL);
        addServerAutofillProfile(getServerAutofillProfile(STREET, CITY, STATE, ZIP));
        SyncTestUtil.triggerSyncAndWaitForCompletion();
        assertClientAutofillProfileCount(0);
    }

    private EntitySpecifics getServerAutofillProfile(
            String street, String city, String state, String zip) {
        AutofillProfileSpecifics profile =
                AutofillProfileSpecifics.newBuilder()
                        .setGuid(GUID)
                        .setAddressHomeLine1(street)
                        .setAddressHomeCity(city)
                        .setAddressHomeState(state)
                        .setAddressHomeZip(zip)
                        .build();
        return EntitySpecifics.newBuilder().setAutofillProfile(profile).build();
    }

    private void addServerAutofillProfile(EntitySpecifics specifics) {
        mSyncTestRule
                .getFakeServerHelper()
                .injectUniqueClientEntity(
                        specifics.getAutofillProfile().getGuid()
                        /* nonUniqueName= */ ,
                        specifics.getAutofillProfile().getGuid()
                        /* clientTag= */ ,
                        specifics);
    }

    private List<Autofill> getClientAutofillProfiles() throws JSONException {
        List<Pair<String, JSONObject>> entities =
                SyncTestUtil.getLocalData(mSyncTestRule.getTargetContext(), AUTOFILL_TYPE);
        List<Autofill> autofills = new ArrayList<Autofill>(entities.size());
        for (Pair<String, JSONObject> entity : entities) {
            String id = entity.first;
            JSONObject profile = entity.second;
            String clientTagHash = "";
            if (profile.has("metadata")) {
                JSONObject metadata = profile.getJSONObject("metadata");
                if (metadata.has("client_tag_hash")) {
                    clientTagHash = metadata.getString("client_tag_hash");
                }
            }
            String street = profile.getString("address_home_line1");
            String city = profile.getString("address_home_city");
            String state = profile.getString("address_home_state");
            String zip = profile.getString("address_home_zip");
            autofills.add(new Autofill(id, clientTagHash, street, city, state, zip));
        }
        return autofills;
    }

    private void assertClientAutofillProfileCount(int count) throws JSONException {
        Assert.assertEquals(
                "There should be " + count + " local autofill profiles.",
                count,
                SyncTestUtil.getLocalData(mSyncTestRule.getTargetContext(), AUTOFILL_TYPE).size());
    }

    private void assertServerAutofillProfileCountWithName(int count, String name) {
        Assert.assertTrue(
                "Expected " + count + " server autofill profiles with name " + name + ".",
                mSyncTestRule
                        .getFakeServerHelper()
                        .verifyEntityCountByTypeAndName(count, DataType.AUTOFILL_PROFILE, name));
    }

    private void waitForClientAutofillProfileCount(int count) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        Criteria.checkThat(
                                SyncTestUtil.getLocalData(
                                                mSyncTestRule.getTargetContext(), AUTOFILL_TYPE)
                                        .size(),
                                Matchers.is(count));
                    } catch (JSONException ex) {
                        throw new CriteriaNotSatisfiedException(ex);
                    }
                },
                SyncTestUtil.TIMEOUT_MS,
                SyncTestUtil.INTERVAL_MS);
    }
}
