// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.guided_browsing;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill_assistant.guided_browsing.parse_xml.AssistantParseSingleTagXmlUtil;

/**
 * Tests for parsing single tag XML.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AssistantParseSingleTagXmlTest {
    @Test
    @MediumTest
    public void testIsXmlSigned() {
        Assert.assertTrue(AssistantParseSingleTagXmlUtil.isXmlSigned("0123456789"));
        Assert.assertFalse(
                AssistantParseSingleTagXmlUtil.isXmlSigned("<?xml version='1.0' encoding='UTF-8'?>"
                        + "<PersonData />"));
    }

    @Test
    @MediumTest
    public void testSuccessfulXmlParsing() {
        String[] attributeValues = AssistantParseSingleTagXmlUtil.extractValuesFromSingleTagXml(
                "<?xml version='1.0' encoding='UTF-8'?>"
                        + "<PersonData id='1234' name='XYZ'>"
                        + "</PersonData>",
                /* attributes= */ new String[] {"id", "name"});

        Assert.assertArrayEquals(attributeValues, new String[] {"1234", "XYZ"});

        attributeValues = AssistantParseSingleTagXmlUtil.extractValuesFromSingleTagXml(
                "<?xml version='1.0' encoding='UTF-8'?>"
                        + "<PersonData id='1234' name='XYZ' />",
                /* attributes= */ new String[] {"id", "name"});

        Assert.assertArrayEquals(attributeValues, new String[] {"1234", "XYZ"});
    }

    @Test
    @MediumTest
    public void testAttributeNotFoundInXml() {
        // Test that an empty array is returned when an attribute is not found in the XML.
        String[] attributeValues = AssistantParseSingleTagXmlUtil.extractValuesFromSingleTagXml(
                "<?xml version='1.0' encoding='UTF-8'?>"
                        + "<PersonData id='1234' name='XYZ'>"
                        + "</PersonData>",
                /* attributes= */ new String[] {"id", "address"});

        Assert.assertArrayEquals(attributeValues, new String[0]);
    }

    @Test
    @MediumTest
    public void testInputIsNotXml() {
        // Test that an empty array is returned when the given xmlString is not XML.
        String[] attributeValues =
                AssistantParseSingleTagXmlUtil.extractValuesFromSingleTagXml("not xml",
                        /* attributes= */ new String[] {"id"});

        Assert.assertArrayEquals(attributeValues, new String[0]);
    }

    @Test
    @MediumTest
    public void testInputIsNotSingleTagXml() {
        // Test that an empty array is returned when the xmlString is not a single tag XML.
        String[] attributeValues = AssistantParseSingleTagXmlUtil.extractValuesFromSingleTagXml(
                "<?xml version='1.0' encoding='UTF-8'?>"
                        + "<PersonData id='1234' name='XYZ'>"
                        + " <ChildData name='ABC'/>"
                        + "</PersonData>",
                /* attributes= */ new String[] {"id"});

        Assert.assertArrayEquals(attributeValues, new String[0]);

        attributeValues = AssistantParseSingleTagXmlUtil.extractValuesFromSingleTagXml(
                "<?xml version='1.0' encoding='UTF-8'?>"
                        + "<PersonData id='1234' name='XYZ' />"
                        + "<ChildData name='ABC'/>",
                /* attributes= */ new String[] {"id"});

        Assert.assertArrayEquals(attributeValues, new String[0]);
    }
}
