// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.settings.SettingsFragment;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Modifier;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.jar.JarEntry;
import java.util.jar.JarFile;
import java.util.stream.Collectors;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;

/**
 * Unit tests for settings enums defined in tools/metrics/histograms/metadata/settings/enums.xml.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SettingsEnumsTest {
    private static final String ENUMS_FILE_PATH =
            "/tools/metrics/histograms/metadata/settings/enums.xml";

    @Test
    public void testAllSettingsFragmentSubclassesAreInEnum() throws Exception {
        Map<Integer, String> fragmentHashes = getFragmentHashesFromEnumsXml();

        Set<String> foundFragments = findAllSettingsFragmentSubclasses();
        assertFalse(
                "Failed to find any PreferenceFragmentCompat subclasses.",
                foundFragments.isEmpty());

        Set<String> missingFragments =
                foundFragments.stream()
                        .filter(
                                name ->
                                        fragmentHashes.get(name.hashCode()) == null
                                                || !fragmentHashes
                                                        .get(name.hashCode())
                                                        .equals(name))
                        .collect(Collectors.toSet());

        assertTrue(
                "The following entries for AndroidSettingsFragmentHashes are missing in"
                        + " tools/metrics/histograms/metadata/settings/enums.xml:\n"
                        + missingFragments.stream()
                                .map(
                                        fragment ->
                                                String.format(
                                                        Locale.ENGLISH,
                                                        "<int value=\"%d\" label=\"%s\"/>",
                                                        fragment.hashCode(),
                                                        fragment))
                                .collect(Collectors.joining("\n")),
                missingFragments.isEmpty());
    }

    private Set<String> findAllSettingsFragmentSubclasses() throws IOException {
        Set<String> fragmentSimpleNames = new HashSet<>();
        String pathSeparator = System.getProperty("path.separator");
        String[] classPathEntries = System.getProperty("java.class.path").split(pathSeparator);
        ClassLoader classLoader = getClass().getClassLoader();

        for (String entry : classPathEntries) {
            if (entry.endsWith(".jar")) {
                if (entry.contains("/components/page_info/")) {
                    // PageInfo fragments are not used in settings.
                    continue;
                }
                if (entry.contains("/junit")) {
                    // Skip test classes.
                    continue;
                }
                processJarFile(entry, classLoader, fragmentSimpleNames);
            }
        }
        return fragmentSimpleNames;
    }

    private void processJarFile(
            String path, ClassLoader classLoader, Set<String> fragmentSimpleNames)
            throws IOException {
        try (JarFile jarFile = new JarFile(path)) {
            Enumeration<JarEntry> entries = jarFile.entries();
            while (entries.hasMoreElements()) {
                JarEntry jarEntry = entries.nextElement();
                if (jarEntry.getName().endsWith(".class")) {
                    String className = jarEntry.getName().replace('/', '.').replace(".class", "");
                    checkAndAddClass(className, classLoader, fragmentSimpleNames);
                }
            }
        }
    }

    private void checkAndAddClass(
            String className, ClassLoader classLoader, Set<String> fragmentSimpleNames) {
        // Only check classes in expected packages to speed things up and avoid unrelated classes.
        if (!className.startsWith("org.chromium")) {
            return;
        }
        try {
            Class<?> clazz = Class.forName(className, false, classLoader);
            if (SettingsFragment.class.isAssignableFrom(clazz)
                    && !clazz.isInterface()
                    && !Modifier.isAbstract(clazz.getModifiers())) {
                fragmentSimpleNames.add(clazz.getSimpleName());
            }
        } catch (ClassNotFoundException | NoClassDefFoundError e) {
            // It's possible to encounter classes that cannot be loaded in the test environment.
            // We can ignore these as they are unlikely to be the settings fragments we are
            // looking for.
        }
    }

    private Map<Integer, String> getFragmentHashesFromEnumsXml() throws Exception {
        File file = new File(System.getProperty("dir.source.root") + ENUMS_FILE_PATH);
        assertTrue("Enums XML file not found: " + file.getAbsolutePath(), file.exists());

        DocumentBuilderFactory dbFactory = DocumentBuilderFactory.newInstance();
        DocumentBuilder dBuilder = dbFactory.newDocumentBuilder();
        Document doc;
        try (InputStream is = new FileInputStream(file)) {
            doc = dBuilder.parse(is);
        }

        NodeList enumsList = doc.getElementsByTagName("enums");
        assertEquals("Expected one <enums> tag", 1, enumsList.getLength());

        Node enumsNode = enumsList.item(0);
        NodeList enumNodes = enumsNode.getChildNodes();

        for (int i = 0; i < enumNodes.getLength(); i++) {
            Node node = enumNodes.item(i);
            if (node.getNodeType() == Node.ELEMENT_NODE) {
                Element enumElement = (Element) node;
                if ("AndroidSettingsFragmentHashes".equals(enumElement.getAttribute("name"))) {
                    Map<Integer, String> fragmentHashes = new HashMap<>();
                    NodeList intNodes = enumElement.getElementsByTagName("int");
                    for (int j = 0; j < intNodes.getLength(); j++) {
                        Node intNode = intNodes.item(j);
                        if (intNode.getNodeType() == Node.ELEMENT_NODE) {
                            Element intElement = (Element) intNode;
                            fragmentHashes.put(
                                    Integer.parseInt(intElement.getAttribute("value")),
                                    intElement.getAttribute("label"));
                        }
                    }
                    return fragmentHashes;
                }
            }
        }
        throw new AssertionError("Did not find AndroidSettingsFragmentHashes enum");
    }
}
