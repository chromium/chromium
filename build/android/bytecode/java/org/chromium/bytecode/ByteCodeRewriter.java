// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.ClassWriter;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

/**
 * Base class for scripts that perform bytecode modifications on a jar file.
 */
public abstract class ByteCodeRewriter {
    private static final String CLASS_FILE_SUFFIX = ".class";

    public void rewrite(File inputJar, File outputJar) throws IOException {
        if (!inputJar.exists()) {
            throw new FileNotFoundException("Input jar not found: " + inputJar.getPath());
        }
        try (InputStream inputStream = new BufferedInputStream(new FileInputStream(inputJar))) {
            try (OutputStream outputStream = new FileOutputStream(outputJar)) {
                processZip(inputStream, outputStream);
            }
        }
    }

    /** Returns true if the class at the given path in the archive should be rewritten. */
    protected abstract boolean shouldRewriteClass(String classPath);

    /**
     * Returns true if the class at the given {@link ClassReader} should be rewritten.
     */
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
        try (ZipOutputStream zipOutputStream = new ZipOutputStream(outputStream)) {
            ZipInputStream zipInputStream = new ZipInputStream(inputStream);
            ZipEntry entry;
            while ((entry = zipInputStream.getNextEntry()) != null) {
                ByteArrayOutputStream buffer = new ByteArrayOutputStream();
                boolean handled = processClassEntry(entry, zipInputStream, buffer);
                if (handled) {
                    ZipEntry newEntry = new ZipEntry(entry.getName());
                    zipOutputStream.putNextEntry(newEntry);
                    zipOutputStream.write(buffer.toByteArray(), 0, buffer.size());
                } else {
                    zipOutputStream.putNextEntry(entry);
                    zipInputStream.transferTo(zipOutputStream);
                }
            }
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
            ClassWriter writer = new ClassWriter(reader, ClassWriter.COMPUTE_FRAMES);
            ClassVisitor classVisitor = writer;
            if (shouldRewriteClass(reader)) {
                classVisitor = getClassVisitorForClass(entry.getName(), writer);
            }
            reader.accept(classVisitor, ClassReader.EXPAND_FRAMES);

            writer.visitEnd();
            byte[] classData = writer.toByteArray();
            outputStream.write(classData, 0, classData.length);
            return true;
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }
}
