// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui.util;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.devui.util.ComponentInfo;
import org.chromium.android_webview.devui.util.ComponentsInfoLoader;
import org.chromium.android_webview.services.ComponentsProviderPathUtil;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.base.FileUtils;
import org.chromium.base.test.util.Batch;

import java.io.File;
import java.util.ArrayList;

/**
 * Unit tests for ComponentsInfoLoader.
 */
@RunWith(AwJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ComponentsInfoLoaderTest {
    private static File sComponentsDownloadDir =
            new File(ComponentsProviderPathUtil.getComponentUpdateServiceDirectoryPath());

    @BeforeClass
    public static void deleteOriginalComponentsDownloadDir() {
        if (sComponentsDownloadDir.exists()) {
            Assert.assertTrue(FileUtils.recursivelyDeleteFile(sComponentsDownloadDir, null));
        }
    }

    @After
    public void tearDown() {
        if (sComponentsDownloadDir.exists()) {
            Assert.assertTrue(FileUtils.recursivelyDeleteFile(sComponentsDownloadDir, null));
        }
    }

    @Test
    @SmallTest
    public void testMultipleComponents_withOneVersionSubDirectory() {
        ComponentInfo[] expectedList =
                new ComponentInfo[] {new ComponentInfo("MockComponent A", "1.0.2.1"),
                        new ComponentInfo("MockComponent B", "2021.1.2.1")};

        for (ComponentInfo mockComponent : expectedList) {
            new File(sComponentsDownloadDir,
                    mockComponent.getComponentName() + "/" + mockComponent.getComponentVersion())
                    .mkdirs();
        }

        ComponentsInfoLoader componentsInfoLoader = new ComponentsInfoLoader();
        ArrayList<ComponentInfo> retrievedComponentsInfoList =
                componentsInfoLoader.getComponentsInfo();

        Assert.assertArrayEquals(expectedList, retrievedComponentsInfoList.toArray());
    }

    @Test
    @SmallTest
    public void testComponentsDownloadDirectory_isEmpty() {
        sComponentsDownloadDir.mkdirs();

        ComponentsInfoLoader componentsInfoLoader = new ComponentsInfoLoader();
        ArrayList<ComponentInfo> retrievedComponentsInfoList =
                componentsInfoLoader.getComponentsInfo();

        Assert.assertEquals(
                "ComponentInfo list should be empty", 0, retrievedComponentsInfoList.size());
    }

    @Test
    @SmallTest
    public void testComponentsDownloadDirectory_doesNotExist() {
        ComponentsInfoLoader componentsInfoLoader = new ComponentsInfoLoader();
        ArrayList<ComponentInfo> retrievedComponentsInfoList =
                componentsInfoLoader.getComponentsInfo();

        Assert.assertEquals(
                "ComponentInfo list should be empty", 0, retrievedComponentsInfoList.size());
    }

    @Test
    @SmallTest
    public void testVersionSubDirectory_doesNotExist() {
        new File(sComponentsDownloadDir, "MockComponent A").mkdirs();

        ComponentsInfoLoader componentsInfoLoader = new ComponentsInfoLoader();
        ArrayList<ComponentInfo> retrievedComponentsInfoList =
                componentsInfoLoader.getComponentsInfo();

        Assert.assertEquals("Expected exactly 1 component", 1, retrievedComponentsInfoList.size());
        Assert.assertEquals(
                "MockComponent A", retrievedComponentsInfoList.get(0).getComponentName());
        Assert.assertEquals("", retrievedComponentsInfoList.get(0).getComponentVersion());
    }
}
