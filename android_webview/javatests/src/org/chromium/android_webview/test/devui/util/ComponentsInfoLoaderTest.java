// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui.util;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;

import org.chromium.android_webview.devui.util.ComponentInfo;
import org.chromium.android_webview.devui.util.ComponentsInfoLoader;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.base.test.util.Batch;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;

/** Unit tests for ComponentsInfoLoader. */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(EITHER_PROCESS) // These are unit tests
@Batch(Batch.UNIT_TESTS)
public class ComponentsInfoLoaderTest {
    @Rule public TemporaryFolder mTempDir = new TemporaryFolder();

    @Test
    @SmallTest
    public void testMultipleComponents_withOneVersionSubDirectory() throws IOException {
        ComponentInfo[] expectedList =
                new ComponentInfo[] {
                    new ComponentInfo("MockComponent A", "1.0.2.1"),
                    new ComponentInfo("MockComponent B", "2021.1.2.1")
                };

        for (ComponentInfo mockComponent : expectedList) {
            mTempDir.newFolder(
                    mockComponent.getComponentName(), mockComponent.getComponentVersion());
        }

        ComponentsInfoLoader componentsInfoLoader = new ComponentsInfoLoader(mTempDir.getRoot());
        ArrayList<ComponentInfo> retrievedComponentsInfoList =
                componentsInfoLoader.getComponentsInfo();

        Assert.assertArrayEquals(expectedList, retrievedComponentsInfoList.toArray());
    }

    @Test
    @SmallTest
    public void testComponentsDownloadDirectory_isEmpty() throws IOException {
        ComponentsInfoLoader componentsInfoLoader = new ComponentsInfoLoader(mTempDir.getRoot());
        ArrayList<ComponentInfo> retrievedComponentsInfoList =
                componentsInfoLoader.getComponentsInfo();

        Assert.assertEquals(
                "ComponentInfo list should be empty", 0, retrievedComponentsInfoList.size());
    }

    @Test
    @SmallTest
    public void testComponentsDownloadDirectory_doesNotExist() throws IOException {
        ComponentsInfoLoader componentsInfoLoader =
                new ComponentsInfoLoader(new File(mTempDir.getRoot(), "nonexistent"));
        ArrayList<ComponentInfo> retrievedComponentsInfoList =
                componentsInfoLoader.getComponentsInfo();

        Assert.assertEquals(
                "ComponentInfo list should be empty", 0, retrievedComponentsInfoList.size());
    }

    @Test
    @SmallTest
    public void testVersionSubDirectory_doesNotExist() throws IOException {
        mTempDir.newFolder("MockComponent A");

        ComponentsInfoLoader componentsInfoLoader = new ComponentsInfoLoader(mTempDir.getRoot());
        ArrayList<ComponentInfo> retrievedComponentsInfoList =
                componentsInfoLoader.getComponentsInfo();

        Assert.assertEquals("Expected exactly 1 component", 1, retrievedComponentsInfoList.size());
        Assert.assertEquals(
                "MockComponent A", retrievedComponentsInfoList.get(0).getComponentName());
        Assert.assertEquals("", retrievedComponentsInfoList.get(0).getComponentVersion());
    }
}
