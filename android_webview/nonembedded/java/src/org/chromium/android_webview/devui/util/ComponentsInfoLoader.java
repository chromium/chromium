// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui.util;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;

/**
 * A Controller class which iterates over the directory where components are downloaded to retrieve
 * component name and version for each component.
 */
public class ComponentsInfoLoader {
    private final File mComponentsDir;

    /**
     * @param componentsDir the top directory where components are downloaded {@link
     *         ComponentsProviderPathUtil#getComponentUpdateServiceDirectoryPath}.
     */
    public ComponentsInfoLoader(File componentsDir) {
        mComponentsDir = componentsDir;
    }

    /**
     * @return list of {@link ComponentInfo} for downloaded components sorted in lexicographical
     *         order.
     */
    public ArrayList<ComponentInfo> getComponentsInfo() {
        ArrayList<ComponentInfo> componentInfoList = new ArrayList<>();
        File[] componentDirectories = mComponentsDir.listFiles();

        if (componentDirectories == null || componentDirectories.length == 0) {
            return componentInfoList;
        }

        Arrays.sort(componentDirectories);

        for (File componentDirectory : componentDirectories) {
            String[] componentVersions = componentDirectory.list();

            // TODO(crbug.com/40779741): Handle multiple versions by sorting semantically and
            // picking out the highest version
            String version =
                    (componentVersions == null || componentVersions.length == 0)
                            ? ""
                            : componentVersions[0];
            String name = componentDirectory.getName();

            componentInfoList.add(new ComponentInfo(name, version));
        }

        return componentInfoList;
    }
}
