// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build;

import java.io.BufferedOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.zip.CRC32;

// Note: Compiled standalone by //third_party/r8/3pp/3pp.py and
// //build/android/bytecode/3pp/3pp.py without //build/android:build_java on the
// classpath, so do not depend on @NullMarked or other build_java classes.
// Changes to this file also require rebuilding and checking in
// //third_party/r8/custom_d8.jar (see //third_party/r8/README.chromium).
/**
 * Writes a ZIP archive with uncompressed (STORED) entries and hermetic timestamps (2001-01-01
 * 00:00:00). Faster than {@link java.util.zip.ZipOutputStream} because callers can reuse existing
 * CRC32 values and entries are not re-hashed on write.
 */
public final class StoredZipWriter implements AutoCloseable {
    // DOS date/time for 2001-01-01 00:00:00.
    private static final int HERMETIC_DOS_TIME = 0x0000;
    private static final int HERMETIC_DOS_DATE = 0x2a21;
    private static final long EXTERNAL_ATTR_FILE = 0100644L << 16;
    private static final long EXTERNAL_ATTR_DIR = (040755L << 16) | 0x10L;
    private static final int COPY_BUFFER_SIZE = 64 * 1024;

    private static final class CdEntry {
        final byte[] mNameBytes;
        final int mFlags;
        final long mCrc;
        final long mSize;
        final long mLocalHeaderOffset;

        CdEntry(byte[] nameBytes, int flags, long crc, long size, long localHeaderOffset) {
            mNameBytes = nameBytes;
            mFlags = flags;
            mCrc = crc;
            mSize = size;
            mLocalHeaderOffset = localHeaderOffset;
        }
    }

    private final OutputStream mOut;
    private final byte[] mHeaderBuf = new byte[46];
    private final CRC32 mCrc32 = new CRC32();
    private final List<CdEntry> mCdEntries = new ArrayList<>();
    private long mPosition;

    public StoredZipWriter(Path outputPath) throws IOException {
        mOut = new BufferedOutputStream(Files.newOutputStream(outputPath), COPY_BUFFER_SIZE);
    }

    private static void putU16(byte[] buf, int off, int val) {
        buf[off] = (byte) val;
        buf[off + 1] = (byte) (val >>> 8);
    }

    private static void putU32(byte[] buf, int off, long val) {
        buf[off] = (byte) val;
        buf[off + 1] = (byte) (val >>> 8);
        buf[off + 2] = (byte) (val >>> 16);
        buf[off + 3] = (byte) (val >>> 24);
    }

    private static int utf8Flags(byte[] nameBytes) {
        for (byte b : nameBytes) {
            if (b < 0) {
                return 0x0800; // Bit 11: UTF-8 filename.
            }
        }
        return 0;
    }

    private void writeLocalHeader(byte[] nameBytes, int flags, long crc, long size)
            throws IOException {
        mCdEntries.add(new CdEntry(nameBytes, flags, crc, size, mPosition));

        putU32(mHeaderBuf, 0, 0x04034b50L);
        putU16(mHeaderBuf, 4, 20);
        putU16(mHeaderBuf, 6, flags);
        putU16(mHeaderBuf, 8, 0); // STORED.
        putU16(mHeaderBuf, 10, HERMETIC_DOS_TIME);
        putU16(mHeaderBuf, 12, HERMETIC_DOS_DATE);
        putU32(mHeaderBuf, 14, crc);
        putU32(mHeaderBuf, 18, size);
        putU32(mHeaderBuf, 22, size);
        putU16(mHeaderBuf, 26, nameBytes.length);
        putU16(mHeaderBuf, 28, 0); // extraLen.
        mOut.write(mHeaderBuf, 0, 30);
        mOut.write(nameBytes);
        mPosition += 30L + nameBytes.length;
    }

    public void writeEntry(String name, byte[] data) throws IOException {
        writeEntry(name, data, /* crc= */ -1);
    }

    /**
     * Writes an uncompressed entry. If {@code crc >= 0}, reuses the provided CRC32 instead of
     * hashing {@code data}.
     */
    public void writeEntry(String name, byte[] data, long crc) throws IOException {
        byte[] nameBytes = name.getBytes(StandardCharsets.UTF_8);
        if (crc < 0) {
            mCrc32.reset();
            mCrc32.update(data);
            crc = mCrc32.getValue();
        }
        writeLocalHeader(nameBytes, utf8Flags(nameBytes), crc, data.length);
        mOut.write(data);
        mPosition += data.length;
    }

    public void finish() throws IOException {
        long cdOffset = mPosition;
        for (CdEntry e : mCdEntries) {
            putU32(mHeaderBuf, 0, 0x02014b50L);
            putU16(mHeaderBuf, 4, 0x0314); // Unix, version 2.0.
            putU16(mHeaderBuf, 6, 20);
            putU16(mHeaderBuf, 8, e.mFlags);
            putU16(mHeaderBuf, 10, 0); // STORED.
            putU16(mHeaderBuf, 12, HERMETIC_DOS_TIME);
            putU16(mHeaderBuf, 14, HERMETIC_DOS_DATE);
            putU32(mHeaderBuf, 16, e.mCrc);
            putU32(mHeaderBuf, 20, e.mSize);
            putU32(mHeaderBuf, 24, e.mSize);
            putU16(mHeaderBuf, 28, e.mNameBytes.length);
            putU16(mHeaderBuf, 30, 0); // extraLen.
            putU16(mHeaderBuf, 32, 0); // commentLen.
            putU16(mHeaderBuf, 34, 0); // diskNum.
            putU16(mHeaderBuf, 36, 0); // internalAttr.
            boolean isDir = e.mNameBytes.length > 0 && e.mNameBytes[e.mNameBytes.length - 1] == '/';
            putU32(mHeaderBuf, 38, isDir ? EXTERNAL_ATTR_DIR : EXTERNAL_ATTR_FILE);
            putU32(mHeaderBuf, 42, e.mLocalHeaderOffset);
            mOut.write(mHeaderBuf, 0, 46);
            mOut.write(e.mNameBytes);
            mPosition += 46L + e.mNameBytes.length;
        }
        long cdSize = mPosition - cdOffset;
        int count = mCdEntries.size();
        if (count > 0xffff || cdOffset > 0xffffffffL || cdSize > 0xffffffffL) {
            throw new IOException("ZIP64 required (entries=" + count + ")");
        }
        putU32(mHeaderBuf, 0, 0x06054b50L);
        putU16(mHeaderBuf, 4, 0);
        putU16(mHeaderBuf, 6, 0);
        putU16(mHeaderBuf, 8, count);
        putU16(mHeaderBuf, 10, count);
        putU32(mHeaderBuf, 12, cdSize);
        putU32(mHeaderBuf, 16, cdOffset);
        putU16(mHeaderBuf, 20, 0);
        mOut.write(mHeaderBuf, 0, 22);
    }

    @Override
    public void close() throws IOException {
        mOut.close();
    }
}
