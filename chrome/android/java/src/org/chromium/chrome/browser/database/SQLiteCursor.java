// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.database;

import android.database.AbstractCursor;
import android.database.CursorWindow;

import org.chromium.base.LifetimeAssert;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

import java.sql.Types;

/**
 * This class exposes the query result from native side.
 */
public class SQLiteCursor extends AbstractCursor {
    private static final String TAG = "SQLiteCursor";
    // Used by JNI.
    private long mNativeSQLiteCursor;

    // The count of result rows.
    private int mCount = -1;

    private int[] mColumnTypes;

    private final Object mColumnTypeLock = new Object();
    private final Object mDestoryNativeLock = new Object();

    // The belows are the locks for those methods that need wait for
    // the callback result in native side.
    private final Object mMoveLock = new Object();
    private final Object mGetBlobLock = new Object();

    private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    private SQLiteCursor(long nativeSQLiteCursor) {
        mNativeSQLiteCursor = nativeSQLiteCursor;
    }

    @CalledByNative
    private static SQLiteCursor create(long nativeSQLiteCursor) {
        return new SQLiteCursor(nativeSQLiteCursor);
    }

    @Override
    public int getCount() {
        synchronized (mMoveLock) {
            if (mCount == -1)
                mCount = SQLiteCursorJni.get().getCount(mNativeSQLiteCursor, SQLiteCursor.this);
        }
        return mCount;
    }

    @Override
    public String[] getColumnNames() {
        return SQLiteCursorJni.get().getColumnNames(mNativeSQLiteCursor, SQLiteCursor.this);
    }

    @Override
    public String getString(int column) {
        return SQLiteCursorJni.get().getString(mNativeSQLiteCursor, SQLiteCursor.this, column);
    }

    @Override
    public short getShort(int column) {
        return (short) SQLiteCursorJni.get().getInt(mNativeSQLiteCursor, SQLiteCursor.this, column);
    }

    @Override
    public int getInt(int column) {
        return SQLiteCursorJni.get().getInt(mNativeSQLiteCursor, SQLiteCursor.this, column);
    }

    @Override
    public long getLong(int column) {
        return SQLiteCursorJni.get().getLong(mNativeSQLiteCursor, SQLiteCursor.this, column);
    }

    @Override
    public float getFloat(int column) {
        return (float) SQLiteCursorJni.get().getDouble(
                mNativeSQLiteCursor, SQLiteCursor.this, column);
    }

    @Override
    public double getDouble(int column) {
        return SQLiteCursorJni.get().getDouble(mNativeSQLiteCursor, SQLiteCursor.this, column);
    }

    @Override
    public boolean isNull(int column) {
        return SQLiteCursorJni.get().isNull(mNativeSQLiteCursor, SQLiteCursor.this, column);
    }

    @Override
    public void close() {
        super.close();
        synchronized (mDestoryNativeLock) {
            if (mNativeSQLiteCursor != 0) {
                SQLiteCursorJni.get().destroy(mNativeSQLiteCursor, SQLiteCursor.this);
                mNativeSQLiteCursor = 0;
                LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
            }
        }
    }

    @Override
    public boolean onMove(int oldPosition, int newPosition) {
        synchronized (mMoveLock) {
            SQLiteCursorJni.get().moveTo(mNativeSQLiteCursor, SQLiteCursor.this, newPosition);
        }
        return super.onMove(oldPosition, newPosition);
    }

    @Override
    public byte[] getBlob(int column) {
        synchronized (mGetBlobLock) {
            return SQLiteCursorJni.get().getBlob(mNativeSQLiteCursor, SQLiteCursor.this, column);
        }
    }

    @Deprecated
    public boolean supportsUpdates() {
        return false;
    }

    @Override
    public void fillWindow(int position, CursorWindow window) {
        if (position < 0 || position > getCount()) {
            return;
        }
        window.acquireReference();
        try {
            int oldpos = getPosition();
            moveToPosition(position - 1);
            window.clear();
            window.setStartPosition(position);
            int columnNum = getColumnCount();
            window.setNumColumns(columnNum);
            while (moveToNext() && window.allocRow()) {
                int pos = getPosition();
                for (int i = 0; i < columnNum; i++) {
                    boolean hasRoom = true;
                    switch (getColumnType(i)) {
                        case Types.DOUBLE:
                            hasRoom = fillRow(window, Double.valueOf(getDouble(i)), pos, i);
                            break;
                        case Types.NUMERIC:
                            hasRoom = fillRow(window, Long.valueOf(getLong(i)), pos, i);
                            break;
                        case Types.BLOB:
                            hasRoom = fillRow(window, getBlob(i), pos, i);
                            break;
                        case Types.LONGVARCHAR:
                            hasRoom = fillRow(window, getString(i), pos, i);
                            break;
                        case Types.NULL:
                            hasRoom = fillRow(window, null, pos, i);
                            break;
                        default:
                            // Ignore an unknown type.
                    }
                    if (!hasRoom) {
                        break;
                    }
                }
            }
            moveToPosition(oldpos);
        } catch (IllegalStateException e) {
            // simply ignore it
        } finally {
            window.releaseReference();
        }
    }

    /**
     * Fill row with the given value. If the value type is other than Long,
     * String, byte[] or Double, the NULL will be filled.
     *
     * @return true if succeeded, false if window is full.
     */
    private boolean fillRow(CursorWindow window, Object value, int pos, int column) {
        if (putValue(window, value, pos, column)) {
            return true;
        } else {
            window.freeLastRow();
            return false;
        }
    }

    /**
     * Put the value in given window. If the value type is other than Long,
     * String, byte[] or Double, the NULL will be filled.
     *
     * @return true if succeeded.
     */
    private boolean putValue(CursorWindow window, Object value, int pos, int column) {
        if (value == null) {
            return window.putNull(pos, column);
        } else if (value instanceof Long) {
            return window.putLong((Long) value, pos, column);
        } else if (value instanceof String) {
            return window.putString((String) value, pos, column);
        } else if (value instanceof byte[] && ((byte[]) value).length > 0) {
            return window.putBlob((byte[]) value, pos, column);
        } else if (value instanceof Double) {
            return window.putDouble((Double) value, pos, column);
        } else {
            return window.putNull(pos, column);
        }
    }

    /**
     * @param index the column index.
     * @return the column type from cache or native side.
     */
    private int getColumnType(int index) {
        synchronized (mColumnTypeLock) {
            if (mColumnTypes == null) {
                int columnCount = getColumnCount();
                mColumnTypes = new int[columnCount];
                for (int i = 0; i < columnCount; i++) {
                    mColumnTypes[i] = SQLiteCursorJni.get().getColumnType(
                            mNativeSQLiteCursor, SQLiteCursor.this, i);
                }
            }
        }
        return mColumnTypes[index];
    }

    @NativeMethods
    interface Natives {
        void destroy(long nativeSQLiteCursor, SQLiteCursor caller);
        int getCount(long nativeSQLiteCursor, SQLiteCursor caller);
        String[] getColumnNames(long nativeSQLiteCursor, SQLiteCursor caller);
        int getColumnType(long nativeSQLiteCursor, SQLiteCursor caller, int column);
        String getString(long nativeSQLiteCursor, SQLiteCursor caller, int column);
        byte[] getBlob(long nativeSQLiteCursor, SQLiteCursor caller, int column);
        boolean isNull(long nativeSQLiteCursor, SQLiteCursor caller, int column);
        long getLong(long nativeSQLiteCursor, SQLiteCursor caller, int column);
        int getInt(long nativeSQLiteCursor, SQLiteCursor caller, int column);
        double getDouble(long nativeSQLiteCursor, SQLiteCursor caller, int column);
        int moveTo(long nativeSQLiteCursor, SQLiteCursor caller, int newPosition);
    }
}
