// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

import org.objectweb.asm.ClassReader;

import java.io.PrintStream;
import java.util.Collection;
import java.util.Collections;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;
import java.util.function.Consumer;

/**
 * Checks classpaths (given as ClassLoaders) by reading the constant pool of the class file and
 * attempting to load every referenced class. If there are some that are unable to be found, it
 * stores a helpful error message if it knows where it might find them, and exits the program if it
 * can't find the class with any given classpath.
 */
public class ClassPathValidator {
    // Number of warnings to print.
    private static final int MAX_MISSING_CLASS_WARNINGS = 10;
    // Number of missing classes to show per missing jar.
    private static final int MAX_ERRORS_PER_JAR = 2;
    // Map of missing .jar -> Missing class -> Classes that failed.
    // TreeMap so that error messages have sorted list of jars.
    private final Map<String, Map<String, Set<String>>> mDirectErrors =
            Collections.synchronizedMap(new TreeMap<>());
    // Missing classes we only track the first one for each jar.
    // Map of missingClass -> srcClass.
    private final Map<String, String> mMissingClasses =
            Collections.synchronizedMap(new TreeMap<>());

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
     * @param errorConsumer Called for each missing class.
     */
    private static void validateClassPath(ClassReader classReader, ClassLoader classLoader,
            Consumer<ClassNotLoadedException> errorConsumer) {
        char[] charBuffer = new char[classReader.getMaxStringLength()];
        // According to the Java spec, the constant pool is indexed from 1 to constant_pool_count -
        // 1. See https://docs.oracle.com/javase/specs/jvms/se7/html/jvms-4.html#jvms-4.4
        for (int i = 1; i < classReader.getItemCount(); i++) {
            int offset = classReader.getItem(i);
            // Class entries correspond to 7 in the constant pool
            // https://docs.oracle.com/javase/specs/jvms/se7/html/jvms-4.html#jvms-4.4
            if (offset > 0 && classReader.readByte(offset - 1) == 7) {
                try {
                    validateClass(classLoader, classReader.readUTF8(offset, charBuffer));
                } catch (ClassNotLoadedException e) {
                    errorConsumer.accept(e);
                }
            }
        }
    }

    public void validateFullClassPath(ClassReader classReader, ClassLoader fullClassLoader,
            Set<String> missingClassAllowlist) {
        // Prebuilts only need transitive dependencies checked, not direct dependencies.
        validateClassPath(classReader, fullClassLoader, (e) -> {
            if (!missingClassAllowlist.contains(e.getClassName())) {
                addMissingError(classReader.getClassName(), e.getClassName());
            }
        });
    }

    public void validateDirectClassPath(ClassReader classReader, ClassLoader directClassLoader,
            ClassLoader fullClassLoader, Collection<String> jarsOnlyInFullClassPath,
            Set<String> missingClassAllowlist, boolean verbose) {
        validateClassPath(classReader, directClassLoader, (e) -> {
            try {
                validateClass(fullClassLoader, e.getClassName());
            } catch (ClassNotLoadedException d) {
                if (!missingClassAllowlist.contains(e.getClassName())) {
                    addMissingError(classReader.getClassName(), e.getClassName());
                }
                return;
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
                    addDirectError(jarPath, classReader.getClassName(), e.getClassName());
                    break;
                } catch (ClassNotLoadedException f) {
                }
            }
        });
    }

    private void addMissingError(String srcClass, String missingClass) {
        mMissingClasses.put(missingClass, srcClass);
    }

    private void addDirectError(String jarPath, String srcClass, String missingClass) {
        synchronized (mDirectErrors) {
            Map<String, Set<String>> failedClassesByMissingClass = mDirectErrors.get(jarPath);
            if (failedClassesByMissingClass == null) {
                // TreeMap so that error messages have sorted list of classes.
                failedClassesByMissingClass = new TreeMap<>();
                mDirectErrors.put(jarPath, failedClassesByMissingClass);
            }
            Set<String> failedClasses = failedClassesByMissingClass.get(missingClass);
            if (failedClasses == null) {
                failedClasses = new TreeSet<>();
                failedClassesByMissingClass.put(missingClass, failedClasses);
            }
            failedClasses.add(srcClass);
        }
    }

    public boolean hasErrors() {
        return !mDirectErrors.isEmpty() || !mMissingClasses.isEmpty();
    }

    private static void printValidationError(
            PrintStream out, String gnTarget, Map<String, Set<String>> missingClasses) {
        out.print(" * ");
        out.println(gnTarget);
        int i = 0;
        // The list of missing classes is non-exhaustive because each class that fails to validate
        // reports only the first missing class.
        for (Map.Entry<String, Set<String>> entry : missingClasses.entrySet()) {
            String missingClass = entry.getKey();
            Set<String> filesThatNeededIt = entry.getValue();
            out.print("     * ");
            if (i == MAX_ERRORS_PER_JAR) {
                out.print(String.format(
                        "And %d more...", missingClasses.size() - MAX_ERRORS_PER_JAR));
                break;
            }
            out.print(missingClass.replace('/', '.'));
            out.print(" (needed by ");
            out.print(filesThatNeededIt.iterator().next().replace('/', '.'));
            if (filesThatNeededIt.size() > 1) {
                out.print(String.format(" and %d more", filesThatNeededIt.size() - 1));
            }
            out.println(")");
            i++;
        }
    }

    public void printAll(String gnTarget, Map<String, String> jarToGnTarget) {
        String streamer = "=============================";
        System.err.println();
        System.err.println(streamer + " Dependency Checks Failed " + streamer);
        System.err.println("Target: " + gnTarget);
        if (!mMissingClasses.isEmpty()) {
            int i = 0;
            for (Map.Entry<String, String> entry : mMissingClasses.entrySet()) {
                if (++i > MAX_MISSING_CLASS_WARNINGS) {
                    System.err.println(String.format("... and %d more.",
                            mMissingClasses.size() - MAX_MISSING_CLASS_WARNINGS));
                    break;
                }
                System.err.println(String.format(
                        "Class \"%s\" not found on any classpath. Used by class \"%s\"",
                        entry.getKey(), entry.getValue()));
            }
            System.err.println();
        }
        if (!mDirectErrors.isEmpty()) {
            System.err.println("Direct classpath is incomplete. To fix, add deps on:");
            for (Map.Entry<String, Map<String, Set<String>>> entry : mDirectErrors.entrySet()) {
                printValidationError(
                        System.err, jarToGnTarget.get(entry.getKey()), entry.getValue());
            }
            System.err.println();
        }
    }
}
