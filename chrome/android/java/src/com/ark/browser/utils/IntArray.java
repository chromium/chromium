package com.ark.browser.utils;

import com.android.internal.util.ArrayUtils;

import java.util.Arrays;

public class IntArray {
    private static final int INIT_CAPACITY = 1;

    private int[] mValues = new int[INIT_CAPACITY];
    private int mSize = 0;

    public int get(int index) {
        return mValues[index];
    }

    public void set(int index, int value) {
        mValues[index] = value;
    }

    public void add(int value) {
        ensureCapacity();
        mValues[mSize++] = value;
    }

    public void add(int index, int value) {
        if (index > mSize) {
            throw new ArrayIndexOutOfBoundsException("length=" + mSize + "; index=" + index);
        } else if (index == mSize) {
            add(value);
        } else {
            ensureCapacity();
            System.arraycopy(mValues, index, mValues, index + 1, mSize - index);
            mValues[index] = value;
        }
    }

    public int indexOf(int value) {
        for (int i = 0; i < mSize; i++) {
            if (mValues[i] == value) {
                return i;
            }
        }
        return -1;
    }

    /**
     * Remove
     * @param index value index
     * @return value
     */
    public int removeAt(int index) {
        int value = mValues[index];
        int removeCount = mSize - index - 1;
        if (removeCount > 0) {
            System.arraycopy(mValues, index + 1, mValues, index, removeCount);
        }
        mSize--;
        return value;
    }

    /**
     *
     * @param index
     * @return
     */
    public int[] removeFrom(int index) {
        // TODO check bounds
        int[] removed = Arrays.copyOfRange(mValues, index, mSize);
        if (mSize - index > 3) {
            mValues = Arrays.copyOfRange(mValues, 0, index);
        }
        mSize = index;
        return removed;
    }

//    public int[] removeFrom(int index, int count) {
//        // TODO check bounds
//        int[] removed = Arrays.copyOfRange(mValues, index, index + count + 1);
//        if (count > 3) {
//            mValues = Arrays.copyOfRange(mValues, 0, index);
//        }
//        mSize = index;
//        return removed;
//    }

    /**
     * Remove
     * @param value remove value
     * @return index
     */
    public int remove(int value) {
        for (int i = 0; i < mSize; i++) {
            if (mValues[i] == value) {
                System.arraycopy(mValues, i + 1, mValues, i, mSize - i - 1);
                mSize--;
                return i;
            }
        }
        return -1;
    }

    private void ensureCapacity() {
        if (mValues.length == mSize) {
            int[] temp = new int[mSize + 3];
            System.arraycopy(mValues, 0, temp, 0, mSize);
            mValues = temp;
        }
    }

    public int removeLast() {
        mSize--;
        return mValues[mSize];
    }

    public int size() {
        return mSize;
    }

    public void clear() {
        mSize = 0;
        if (mValues.length != INIT_CAPACITY) mValues = new int[INIT_CAPACITY];
    }
}

