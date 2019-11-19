// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

import org.objectweb.asm.ClassReader;

import java.util.Collection;
import java.util.Collections;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;

/**
 * Checks classpaths (given as ClassLoaders) by reading the constant pool of the class file and
 * attempting to load every referenced class. If there are some that are unable to be found, it
 * stores a helpful error message if it knows where it might find them, and exits the program if it
 * can't find the class with any given classpath.
 */
public class ClassPathValidator {
    // Map of missing .jar -> Missing class -> Classes that failed.
    // TreeMap so that error messages have sorted list of jars.
    private final Map<String, Map<String, Set<String>>> mErrors = new TreeMap<>();

    static class ClassNotLoadedException extends ClassNotFoundException {
        private final String mClassName;

        ClassNotLoadedException(String className, Throwable ex) {
            super("Couldn't load " + className, ex);
            mClassName = className;
        }

        public String getClassName() {
            return mClassName;
        }
    }

    private static void printAndQuit(ClassNotLoadedException e, ClassReader classReader,
            boolean verbose) throws ClassNotLoadedException {
        System.err.println("Class \"" + e.getClassName()
                + "\" not found on any classpath. Used by class \"" + classReader.getClassName()
                + "\"");
        if (verbose) {
            throw e;
        }
        System.exit(1);
    }

    private static void validateClass(ClassLoader classLoader, String className)
            throws ClassNotLoadedException {
        if (className.startsWith("[")) {
            // Dealing with an array type which isn't encoded nicely in the constant pool.
            // For example, [[Lorg/chromium/Class$1;
            className = className.substring(className.lastIndexOf('[') + 1);
            if (className.charAt(0) == 'L' && className.endsWith(";")) {
                className = className.substring(1, className.length() - 1);
            } else {
                // Bailing out if we have an non-class array type.
                // This could be something like [B
                return;
            }
        }
        if (className.matches(".*\\bR(\\$\\w+)?$")) {
            // Resources in R.java files are not expected to be valid at this stage in the build.
            return;
        }
        if (className.matches("^libcore\\b.*")) {
            // libcore exists on devices, but is not included in the Android sdk as it is a private
            // API.
            return;
        }
        try {
            classLoader.loadClass(className.replace('/', '.'));
        } catch (ClassNotFoundException e) {
            throw new ClassNotLoadedException(className, e);
        } catch (NoClassDefFoundError e) {
            // We assume that this is caused by another class that is not going to able to be
            // loaded, so we will skip this and let that class fail with ClassNotFoundException.
        }
    }

    /**
     * Given a .class file, see if every class referenced in the main class' constant pool can be
     * loaded by the given ClassLoader.
     *
     * @param classReader .class file interface for reading the constant pool.
     * @param classLoader classpath you wish to validate.
     * @throws ClassNotLoadedException thrown if it can't load a certain class.
     */
    private static void validateClassPath(ClassReader classReader, ClassLoader classLoader)
            throws ClassNotLoadedException {
        char[] charBuffer = new char[classReader.getMaxStringLength()];
        // According to the Java spec, the constant pool is indexed from 1 to constant_pool_count -
        // 1. See https://docs.oracle.com/javase/specs/jvms/se7/html/jvms-4.html#jvms-4.4
        for (int i = 1; i < classReader.getItemCount(); i++) {
            int offset = classReader.getItem(i);
            // Class entries correspond to 7 in the constant pool
            // https://docs.oracle.com/javase/specs/jvms/se7/html/jvms-4.html#jvms-4.4
            if (offset > 0 && classReader.readByte(offset - 1) == 7) {
                validateClass(classLoader, classReader.readUTF8(offset, charBuffer));
            }
        }
    }

    public void validateClassPathsAndOutput(ClassReader classReader,
            ClassLoader directClassPathClassLoader, ClassLoader fullClassPathClassLoader,
            Collection<String> jarsOnlyInFullClassPath, boolean isPrebuilt, boolean verbose)
            throws ClassNotLoadedException {
        if (isPrebuilt) {
            // Prebuilts only need transitive dependencies checked, not direct dependencies.
            try {
                validateClassPath(classReader, fullClassPathClassLoader);
            } catch (ClassNotLoadedException e) {
                printAndQuit(e, classReader, verbose);
            }
        } else {
            try {
                validateClassPath(classReader, directClassPathClassLoader);
            } catch (ClassNotLoadedException e) {
                try {
                    validateClass(fullClassPathClassLoader, e.getClassName());
                } catch (ClassNotLoadedException d) {
                    printAndQuit(d, classReader, verbose);
                }
                if (verbose) {
                    System.err.println("Class \"" + e.getClassName()
                            + "\" not found in direct dependencies,"
                            + " but found in indirect dependiences.");
                }
                // Iterating through all jars that are in the full classpath but not the direct
                // classpath to find which one provides the class we are looking for.
                for (String jarPath : jarsOnlyInFullClassPath) {
                    try {
                        ClassLoader smallLoader =
                                ByteCodeProcessor.loadJars(Collections.singletonList(jarPath));
                        validateClass(smallLoader, e.getClassName());
                        Map<String, Set<String>> failedClassesByMissingClass = mErrors.get(jarPath);
                        if (failedClassesByMissingClass == null) {
                            // TreeMap so that error messages have sorted list of classes.
                            failedClassesByMissingClass = new TreeMap<>();
                            mErrors.put(jarPath, failedClassesByMissingClass);
                        }
                        Set<String> failedClasses =
                                failedClassesByMissingClass.get(e.getClassName());
                        if (failedClasses == null) {
                            failedClasses = new TreeSet<>();
                            failedClassesByMissingClass.put(e.getClassName(), failedClasses);
                        }
                        failedClasses.add(classReader.getClassName());
                        break;
                    } catch (ClassNotLoadedException f) {
                    }
                }
            }
        }
    }

    public Map<String, Map<String, Set<String>>> getErrors() {
        return mErrors;
    }

    public boolean hasErrors() {
        return !mErrors.isEmpty();
    }
}
