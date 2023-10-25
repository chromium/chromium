// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.webview_ui_test.test.robolectric;

import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.webview_ui_test.test.util.Action;
import org.chromium.webview_ui_test.test.util.CapturedSitesInstructions;

import java.io.IOException;
import java.util.Optional;

/** Unit testing for CapturedSitesInstructions. */
@RunWith(BaseRobolectricTestRunner.class)
public final class CapturedSitesInstructionsTest {
    @Test
    @SmallTest
    public void verifyBuild_failsIfNoTestFile() {
        String mUrl = "missing.test";
        assertThrows(IOException.class, () -> new CapturedSitesInstructions(mUrl));
    }

    // We can't test the loading of the url due to the way file structure works so, all we can do is
    // test json object is read from correctly and output is correct.

    @Test
    @SmallTest
    public void verifyBuild_failsIfTestEmpty() {
        JSONObject test = new JSONObject();
        assertThrows(JSONException.class, () -> new CapturedSitesInstructions(test));
    }

    @Test
    @SmallTest
    public void verifyBuild_succeedsWithOnlyStartingUrl() throws Throwable {
        JSONObject test = new JSONObject();
        test.put("actions", new JSONArray());
        test.put("startingURL", "google.com");
        CapturedSitesInstructions actions = new CapturedSitesInstructions(test);

        Action action = actions.getNextAction();
        assertTrue(action.toString().contains("google.com"));

        action = actions.getNextAction();
        assertNull(actions.getNextAction());
    }

    // Creates CapturedSistes insturctions for LoadPage tests.
    private CapturedSitesInstructions loadPageForceHelper(Optional<Boolean> force)
            throws Throwable {
        JSONObject test = new JSONObject();
        JSONArray jsonActions = new JSONArray();
        test.put("startingURL", "google.com");
        JSONObject obj = new JSONObject();
        obj.put("type", "loadPage");
        obj.put("url", "myUrl");
        if (force.isPresent()) {
            obj.put("force", force.get().toString());
        }
        jsonActions.put(obj);
        test.put("actions", jsonActions);
        return new CapturedSitesInstructions(test);
    }

    @Test
    @SmallTest
    public void verifyBuild_succeedsWithLoadPageForce() throws Throwable {
        CapturedSitesInstructions actions = loadPageForceHelper(Optional.of(true));
        actions.getNextAction(); // Skip startingURL
        Action action = actions.getNextAction();
        assertTrue(action.toString().contains("myUrl"));
        assertTrue(action.toString().contains("Forcing load of"));
    }

    @Test
    @SmallTest
    public void verifyBuild_succeedsWithLoadPageNoForce() throws Throwable {
        CapturedSitesInstructions actions = loadPageForceHelper(Optional.of(false));
        actions.getNextAction(); // Skip startingURL
        Action action = actions.getNextAction();
        assertTrue(action.toString().contains("myUrl"));
        assertTrue(action.toString().contains("Entering"));
    }

    @Test
    @SmallTest
    public void verifyBuild_succeedsWithLoadPageNullForce() throws Throwable {
        CapturedSitesInstructions actions = loadPageForceHelper(Optional.empty());
        actions.getNextAction(); // Skip startingURL
        Action action = actions.getNextAction();
        assertTrue(action.toString().contains("myUrl"));
        assertTrue(action.toString().contains("Entering"));
    }

    @Test
    @SmallTest
    public void verifyBuild_completeTestSucceeds() throws Throwable {
        JSONObject test = new JSONObject();
        JSONArray jsonActions = new JSONArray();
        JSONObject obj;

        test.put("startingURL", "google.com");

        // Load page
        obj = new JSONObject();
        obj.put("type", "loadPage");
        obj.put("url", "myUrl");
        jsonActions.put(obj);
        // Click
        obj = new JSONObject();
        obj.put("type", "click");
        obj.put("selector", "target1");
        jsonActions.put(obj);
        // Autofill
        obj = new JSONObject();
        obj.put("type", "autofill");
        obj.put("selector", "target2");
        jsonActions.put(obj);
        // ValidateField
        obj = new JSONObject();
        obj.put("type", "validateField");
        obj.put("selector", "target3");
        obj.put("expectedValue", "expectedValue1");
        jsonActions.put(obj);
        test.put("actions", jsonActions);
        CapturedSitesInstructions actions = new CapturedSitesInstructions(test);

        Action action;
        action = actions.getNextAction();
        // Use contains to be less sensitive to inconsquential changes to toString() implementation.
        assertTrue(action.toString().contains("google.com"));

        action = actions.getNextAction();
        assertTrue(action.toString().contains("myUrl"));

        action = actions.getNextAction();
        assertTrue(action.toString().contains("target1"));

        action = actions.getNextAction();
        assertTrue(action.toString().contains("target2"));

        action = actions.getNextAction();
        assertTrue(action.toString().contains("target3"));
        assertTrue(action.toString().contains("expectedValue1"));

        assertNull(actions.getNextAction());
    }
}
