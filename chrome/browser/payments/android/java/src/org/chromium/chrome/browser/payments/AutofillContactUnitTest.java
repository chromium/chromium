// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;

import android.content.Context;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.autofill.AutofillProfile;

/** Unit tests for the AutofillContact class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AutofillContactUnitTest {
    private static final String MESSAGE = "message";
    private static final String NAME = "Jon Doe";
    private static final String PHONE = "555-555-555";
    private static final String EMAIL = "jon@doe.com";

    @Test
    public void testIsEqualOrSupersetOf_RequestAllFields() {
        AutofillProfile dummyProfile = AutofillProfile.builder().build();
        Context mockContext = spy(RuntimeEnvironment.application);
        doReturn(MESSAGE).when(mockContext).getString(anyInt());

        AutofillContact contact1 =
                new AutofillContact(
                        mockContext,
                        dummyProfile,
                        NAME,
                        PHONE,
                        EMAIL,
                        ContactEditor.COMPLETE,
                        /* requestName= */ true,
                        /* requestPhone= */ true,
                        /* requestEmail= */ true);
        AutofillContact contact2 =
                new AutofillContact(
                        mockContext,
                        dummyProfile,
                        NAME,
                        PHONE,
                        EMAIL,
                        ContactEditor.COMPLETE,
                        /* requestName= */ true,
                        /* requestPhone= */ true,
                        /* requestEmail= */ true);

        // The return value should be true for identical profiles.
        Assert.assertTrue(contact1.isEqualOrSupersetOf(contact2));

        // The return value should be true if the second profile is missing fields.
        contact2.completeContact("", "", PHONE, EMAIL);
        Assert.assertTrue(contact1.isEqualOrSupersetOf(contact2));
        contact2.completeContact("", NAME, "", EMAIL);
        Assert.assertTrue(contact1.isEqualOrSupersetOf(contact2));
        contact2.completeContact("", NAME, PHONE, "");
        Assert.assertTrue(contact1.isEqualOrSupersetOf(contact2));
        contact2.completeContact("", NAME, "", "");
        Assert.assertTrue(contact1.isEqualOrSupersetOf(contact2));
        contact2.completeContact("", "", PHONE, "");
        Assert.assertTrue(contact1.isEqualOrSupersetOf(contact2));
        contact2.completeContact("", "", "", EMAIL);
        Assert.assertTrue(contact1.isEqualOrSupersetOf(contact2));
        contact2.completeContact("", "", "", "");
        Assert.assertTrue(contact1.isEqualOrSupersetOf(contact2));

        // The return value should be false if one field is different.
        contact2.completeContact("", "diff", PHONE, EMAIL);
        Assert.assertFalse(contact1.isEqualOrSupersetOf(contact2));
        contact2.completeContact("", NAME, "diff", EMAIL);
        Assert.assertFalse(contact1.isEqualOrSupersetOf(contact2));
        contact2.completeContact("", NAME, PHONE, "diff");
        Assert.assertFalse(contact1.isEqualOrSupersetOf(contact2));
    }

    @Test
    public void testIsEqualOrSupersetOf_RequestSomeFields() {
        AutofillProfile dummyProfile = AutofillProfile.builder().build();
        Context mockContext = spy(RuntimeEnvironment.application);
        doReturn(MESSAGE).when(mockContext).getString(anyInt());

        // The merchant does not request a name.
        AutofillContact contact1 =
                new AutofillContact(
                        mockContext,
                        dummyProfile,
                        NAME,
                        PHONE,
                        EMAIL,
                        ContactEditor.COMPLETE,
                        /* requestName= */ false,
                        /* requestPhone= */ true,
                        /* requestEmail= */ true);
        AutofillContact contact2 =
                new AutofillContact(
                        mockContext,
                        dummyProfile,
                        NAME,
                        PHONE,
                        EMAIL,
                        ContactEditor.COMPLETE,
                        /* requestName= */ false,
                        /* requestPhone= */ true,
                        /* requestEmail= */ true);

        // The return value should be true for identical profiles.
        Assert.assertTrue(contact1.isEqualOrSupersetOf(contact2));

        // The return value should be true even if the name is missing.
        contact2.completeContact("", "", PHONE, EMAIL);
        Assert.assertTrue(contact1.isEqualOrSupersetOf(contact2));

        // The return value should be true even if the name is different.
        contact2.completeContact("", "diff", PHONE, EMAIL);
        Assert.assertTrue(contact1.isEqualOrSupersetOf(contact2));
    }

    @Test
    public void testGetRelevanceScore_RequestAllFields() {
        AutofillProfile dummyProfile = AutofillProfile.builder().build();
        Context mockContext = spy(RuntimeEnvironment.application);
        doReturn(MESSAGE).when(mockContext).getString(anyInt());

        // The merchant requests all fields.
        // Since all requested fields are present and valid, The score should be 3.
        AutofillContact contact =
                new AutofillContact(
                        mockContext,
                        dummyProfile,
                        NAME,
                        PHONE,
                        EMAIL,
                        ContactEditor.COMPLETE,
                        /* requestName= */ true,
                        /* requestPhone= */ true,
                        /* requestEmail= */ true);
        Assert.assertEquals(3, contact.getRelevanceScore());

        // The name is not valid, the score should be 2.
        contact =
                new AutofillContact(
                        mockContext,
                        dummyProfile,
                        NAME,
                        PHONE,
                        EMAIL,
                        ContactEditor.INVALID_NAME,
                        /* requestName= */ true,
                        /* requestPhone= */ true,
                        /* requestEmail= */ true);
        Assert.assertEquals(2, contact.getRelevanceScore());

        // The phone is not valid, the score should be 2.
        contact =
                new AutofillContact(
                        mockContext,
                        dummyProfile,
                        NAME,
                        PHONE,
                        EMAIL,
                        ContactEditor.INVALID_PHONE_NUMBER,
                        /* requestName= */ true,
                        /* requestPhone= */ true,
                        /* requestEmail= */ true);
        Assert.assertEquals(2, contact.getRelevanceScore());

        // The email is not valid, the score should be 2.
        contact =
                new AutofillContact(
                        mockContext,
                        dummyProfile,
                        NAME,
                        PHONE,
                        EMAIL,
                        ContactEditor.INVALID_EMAIL,
                        /* requestName= */ true,
                        /* requestPhone= */ true,
                        /* requestEmail= */ true);
        Assert.assertEquals(2, contact.getRelevanceScore());

        // The name and phone are not valid, the score should be 1.
        contact =
                new AutofillContact(
                        mockContext,
                        dummyProfile,
                        NAME,
                        PHONE,
                        EMAIL,
                        ContactEditor.INVALID_NAME | ContactEditor.INVALID_PHONE_NUMBER,
                        /* requestName= */ true,
                        /* requestPhone= */ true,
                        /* requestEmail= */ true);
        Assert.assertEquals(1, contact.getRelevanceScore());

        // The name and email are not valid, the score should be 1.
        contact =
                new AutofillContact(
                        mockContext,
                        dummyProfile,
                        NAME,
                        PHONE,
                        EMAIL,
                        ContactEditor.INVALID_NAME | ContactEditor.INVALID_EMAIL,
                        /* requestName= */ true,
                        /* requestPhone= */ true,
                        /* requestEmail= */ true);
        Assert.assertEquals(1, contact.getRelevanceScore());

        // The phone and email are not valid, the score should be 1.
        contact =
                new AutofillContact(
                        mockContext,
                        dummyProfile,
                        NAME,
                        PHONE,
                        EMAIL,
                        ContactEditor.INVALID_PHONE_NUMBER | ContactEditor.INVALID_EMAIL,
                        /* requestName= */ true,
                        /* requestPhone= */ true,
                        /* requestEmail= */ true);
        Assert.assertEquals(1, contact.getRelevanceScore());

        // The name, phone and email are not valid, the score should be 0.
        contact =
                new AutofillContact(
                        mockContext,
                        dummyProfile,
                        NAME,
                        PHONE,
                        EMAIL,
                        ContactEditor.INVALID_NAME
                                | ContactEditor.INVALID_PHONE_NUMBER
                                | ContactEditor.INVALID_EMAIL,
                        /* requestName= */ true,
                        /* requestPhone= */ true,
                        /* requestEmail= */ true);
        Assert.assertEquals(0, contact.getRelevanceScore());
    }

    @Test
    public void testGetRelevanceScore_RequestSomeFields() {
        AutofillProfile dummyProfile = AutofillProfile.builder().build();
        Context mockContext = spy(RuntimeEnvironment.application);
        doReturn(MESSAGE).when(mockContext).getString(anyInt());

        // The merchant does not request a name.
        // Since all requested fields are present and valid, The score should be 2.
        AutofillContact contact =
                new AutofillContact(
                        mockContext,
                        dummyProfile,
                        NAME,
                        PHONE,
                        EMAIL,
                        ContactEditor.COMPLETE,
                        /* requestName= */ false,
                        /* requestPhone= */ true,
                        /* requestEmail= */ true);
        Assert.assertEquals(2, contact.getRelevanceScore());

        // The name is not valid, the score should still be 2.
        contact =
                new AutofillContact(
                        mockContext,
                        dummyProfile,
                        NAME,
                        PHONE,
                        EMAIL,
                        ContactEditor.INVALID_NAME,
                        /* requestName= */ false,
                        /* requestPhone= */ true,
                        /* requestEmail= */ true);
        Assert.assertEquals(2, contact.getRelevanceScore());

        // The phone is not valid, the score should be 1.
        contact =
                new AutofillContact(
                        mockContext,
                        dummyProfile,
                        NAME,
                        PHONE,
                        EMAIL,
                        ContactEditor.INVALID_PHONE_NUMBER,
                        /* requestName= */ false,
                        /* requestPhone= */ true,
                        /* requestEmail= */ true);
        Assert.assertEquals(1, contact.getRelevanceScore());

        // The email is not valid, the score should be 1.
        contact =
                new AutofillContact(
                        mockContext,
                        dummyProfile,
                        NAME,
                        PHONE,
                        EMAIL,
                        ContactEditor.INVALID_EMAIL,
                        /* requestName= */ false,
                        /* requestPhone= */ true,
                        /* requestEmail= */ true);
        Assert.assertEquals(1, contact.getRelevanceScore());

        // The phone and email are not valid, the score should be 0.
        contact =
                new AutofillContact(
                        mockContext,
                        dummyProfile,
                        NAME,
                        PHONE,
                        EMAIL,
                        ContactEditor.INVALID_PHONE_NUMBER | ContactEditor.INVALID_EMAIL,
                        /* requestName= */ false,
                        /* requestPhone= */ true,
                        /* requestEmail= */ true);
        Assert.assertEquals(0, contact.getRelevanceScore());
    }
}
