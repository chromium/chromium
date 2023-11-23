// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.ClassWriter;

import java.io.BufferedInputStream;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

/** Base class for scripts that perform bytecode modifications on a jar file. */
public abstract class ByteCodeRewriter {
    private static final String CLASS_FILE_SUFFIX = ".class";

    static String[] expandArgs(String[] args) throws IOException {
        if (args.length == 1 && args[0].startsWith("@")) {
            Path path = Paths.get(args[0].substring(1));
            args = Files.readAllLines(path).toArray(new String[0]);
        }
        return args;
    }

    public void rewrite(File inputJar, File outputJar) throws IOException {
        if (!inputJar.exists()) {
            throw new FileNotFoundException("Input jar not found: " + inputJar.getPath());
        }

        try (InputStream inputStream = new BufferedInputStream(new FileInputStream(inputJar));
                OutputStream outputStream = new FileOutputStream(outputJar)) {
            processZip(inputStream, outputStream);
        }
    }

    /** Returns true if the class at the given path in the archive should be rewritten. */
    protected abstract boolean shouldRewriteClass(String classPath);

    /** Returns true if the class at the given {@link ClassReader} should be rewritten. */
    protected boolean shouldRewriteClass(ClassReader classReader) {
        return true;
    }

    /**
     * Returns the ClassVisitor that should be used to modify the bytecode of class at the given
     * path in the archive.
     */
    protected abstract ClassVisitor getClassVisitorForClass(
            String classPath, ClassVisitor delegate);

    private void processZip(InputStream inputStream, OutputStream outputStream) {
        try (ZipOutputStream zipOutputStream = new ZipOutputStream(outputStream);
                ZipInputStream zipInputStream = new ZipInputStream(inputStream)) {
            ZipEntry entry;
            while ((entry = zipInputStream.getNextEntry()) != null) {
                // Get the uncompressed contents of the current zip entry and wrap in an input
                // stream. This is done because ZipInputStreams can't be reset so they can only be
                // read once, and classes that don't need rewriting need to be read twice, first to
                // parse and then to copy.
                byte[] currentEntryBytes = zipInputStream.readAllBytes();
                ByteArrayInputStream currentEntryInputStream =
                        new ByteArrayInputStream(currentEntryBytes);
                ByteArrayOutputStream outputBuffer = new ByteArrayOutputStream();
                boolean handled = processClassEntry(entry, currentEntryInputStream, outputBuffer);

                ZipEntry newEntry = new ZipEntry(entry.getName());
                newEntry.setTime(entry.getTime());
                zipOutputStream.putNextEntry(newEntry);
                if (handled) {
                    zipOutputStream.write(outputBuffer.toByteArray(), 0, outputBuffer.size());
                } else {
                    // processClassEntry may have advanced currentEntryInputStream, so reset it to
                    // copy zip entry contents unmodified.
                    currentEntryInputStream.reset();
                    currentEntryInputStream.transferTo(zipOutputStream);
                }
                zipOutputStream.closeEntry();
            }

            zipOutputStream.finish();
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    private boolean processClassEntry(
            ZipEntry entry, InputStream inputStream, OutputStream outputStream) {
        if (!entry.getName().endsWith(CLASS_FILE_SUFFIX) || !shouldRewriteClass(entry.getName())) {
            return false;
        }
        try {
            ClassReader reader = new ClassReader(inputStream);
            if (!shouldRewriteClass(reader)) {
                return false;
            }
            ClassWriter writer = new ClassWriter(reader, ClassWriter.COMPUTE_FRAMES);
            ClassVisitor classVisitor = getClassVisitorForClass(entry.getName(), writer);
            reader.accept(classVisitor, ClassReader.EXPAND_FRAMES);

            writer.visitEnd();
            byte[] classData = writer.toByteArray();
            outputStream.write(classData, 0, classData.length);
            return true;
        } catch (Throwable e) {
            throw new RuntimeException("Failed when processing " + entry.getName(), e);
        }
    }
}
