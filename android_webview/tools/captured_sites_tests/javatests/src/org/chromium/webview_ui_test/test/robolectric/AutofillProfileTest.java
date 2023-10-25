// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.webview_ui_test.test.robolectric;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThrows;

import androidx.test.filters.SmallTest;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.webview_ui_test.test.util.AutofillProfile;

import java.io.IOException;

/** Unit testing for CapturedSitesInstructions. */
@RunWith(BaseRobolectricTestRunner.class)
public final class AutofillProfileTest {
    @Test
    @SmallTest
    public void verifyAutofillProfile_testCompleteBuilds() throws Throwable {
        // (crbug/1473318) Add utilities to create these JSONs and CapturedSitesInstructionsTests.
        JSONObject test = new JSONObject();
        JSONArray profile = new JSONArray();
        JSONObject obj;
        obj = new JSONObject();
        obj.put("type", "name");
        obj.put("value", "John");
        profile.put(obj);

        obj = new JSONObject();
        obj.put("type", "age");
        obj.put("value", "30");
        profile.put(obj);

        obj = new JSONObject();
        obj.put("type", "job");
        obj.put("value", "Professor");
        profile.put(obj);

        test.put("autofillProfile", profile);

        AutofillProfile johnProfile = new AutofillProfile(test);
        assertEquals(johnProfile.profileMap.get("name"), "John");
        assertEquals(johnProfile.profileMap.get("age"), "30");
        assertEquals(johnProfile.profileMap.get("job"), "Professor");
    }

    @Test
    @SmallTest
    public void verifyAutofillProfile_missingFileFails() {
        String mUrl = "nowhere.profile";
        assertThrows(IOException.class, () -> new AutofillProfile(mUrl));
    }

    // We can't test the loading of the url due to the way file structure works so, all we can do is
    // test json object is read from correctly and output is correct.
    @Test
    @SmallTest
    public void verifyAutofillProfile_noProfileFails() throws Throwable {
        JSONObject test = new JSONObject();
        assertThrows(JSONException.class, () -> new AutofillProfile(test));
    }

    @Test
    @SmallTest
    public void verifyAutofillProfile_emptyProfileFails() throws Throwable {
        JSONObject test = new JSONObject();
        JSONArray profile = new JSONArray();
        test.put("autofillProfile", profile);
        assertThrows(IOException.class, () -> new AutofillProfile(test));
    }

    @Test
    @SmallTest
    public void verifyAutofillProfile_duplicateFieldFails() throws Throwable {
        JSONObject test = new JSONObject();
        JSONArray profile = new JSONArray();
        JSONObject obj;
        obj = new JSONObject();
        obj.put("type", "name");
        obj.put("value", "John");
        profile.put(obj);

        obj = new JSONObject();
        obj.put("type", "name");
        obj.put("value", "Emily");
        profile.put(obj);

        test.put("autofillProfile", profile);
        assertThrows(IOException.class, () -> new AutofillProfile(test));
    }
}
