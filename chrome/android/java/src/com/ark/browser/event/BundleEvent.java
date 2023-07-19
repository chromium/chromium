package com.ark.browser.event;

import android.os.Bundle;
import android.os.IBinder;
import android.os.Parcelable;
import android.util.Size;
import android.util.SizeF;
import android.util.SparseArray;

import androidx.annotation.Nullable;

import com.zpj.bus.ZBus;

import java.io.Serializable;
import java.util.ArrayList;

public class BundleEvent {

    public static final int ACTION_GO_TO_BROWSER = 0;
    public static final int ACTION_ADD_TO_HOMEPAGE = 1;
    public static final int ACTION_REMOVE_FROM_HOMEPAGE = 2;

    private final int mAction;
    private final Bundle mBundle;

    private BundleEvent(int action, Bundle bundle) {
        mAction = action;
        mBundle = bundle;
    }

    public static Builder with(int action) {
        return new Builder(action);
    }

    public int getAction() {
        return mAction;
    }

    public Bundle getBundle() {
        return mBundle;
    }

    public String getString(String key) {
        return mBundle.getString(key);
    }

    public String getString(String key, String defaultVaue) {
        return mBundle.getString(key, defaultVaue);
    }

    public String[] getStringArray(String key) {
        return mBundle.getStringArray(key);
    }

    public boolean getBoolean(String key) {
        return mBundle.getBoolean(key);
    }

    public boolean getBoolean(String key, boolean defaultVaue) {
        return mBundle.getBoolean(key, defaultVaue);
    }

    public boolean[] getBooleanArray(String key) {
        return mBundle.getBooleanArray(key);
    }

    public byte getByte(String key) {
        return mBundle.getByte(key);
    }

    public Byte getByte(String key, byte defaultValue) {
        return mBundle.getByte(key, defaultValue);
    }

    public char getChar(String key) {
        return mBundle.getChar(key);
    }

    public char getChar(String key, char defaultValue) {
        return mBundle.getChar(key, defaultValue);
    }

    public short getShort(String key) {
        return mBundle.getShort(key);
    }

    public short getShort(String key, short defaultValue) {
        return mBundle.getShort(key, defaultValue);
    }

    public float getFloat(String key) {
        return mBundle.getFloat(key);
    }

    public float getFloat(String key, float defaultValue) {
        return mBundle.getFloat(key, defaultValue);
    }

    @Nullable
    public CharSequence getCharSequence(@Nullable String key) {
        return mBundle.getCharSequence(key);
    }

    public CharSequence getCharSequence(@Nullable String key, CharSequence defaultValue) {
        return mBundle.getCharSequence(key, defaultValue);
    }

    @Nullable
    public Size getSize(@Nullable String key) {
        return mBundle.getSize(key);
    }

    @Nullable
    public SizeF getSizeF(@Nullable String key) {
        return mBundle.getSizeF(key);
    }

    @Nullable
    public Bundle getBundle(@Nullable String key) {
        return mBundle.getBundle(key);
    }

    @Nullable
    public <T extends Parcelable> T getParcelable(@Nullable String key) {
        return mBundle.getParcelable(key);
    }

    @Nullable
    public Parcelable[] getParcelableArray(@Nullable String key) {
        return mBundle.getParcelableArray(key);
    }

    @Nullable
    public <T extends Parcelable> ArrayList<T> getParcelableArrayList(@Nullable String key) {
        return mBundle.getParcelableArrayList(key);
    }

    @Nullable
    public <T extends Parcelable> SparseArray<T> getSparseParcelableArray(@Nullable String key) {
        return mBundle.getSparseParcelableArray(key);
    }

    @Nullable
    public Serializable getSerializable(@Nullable String key) {
        return mBundle.getSerializable(key);
    }

    @Nullable
    public ArrayList<Integer> getIntegerArrayList(@Nullable String key) {
        return mBundle.getIntegerArrayList(key);
    }

    @Nullable
    public ArrayList<String> getStringArrayList(@Nullable String key) {
        return mBundle.getStringArrayList(key);
    }

    @Nullable
    public ArrayList<CharSequence> getCharSequenceArrayList(@Nullable String key) {
        return mBundle.getCharSequenceArrayList(key);
    }

    @Nullable
    public byte[] getByteArray(@Nullable String key) {
        return mBundle.getByteArray(key);
    }

    @Nullable
    public short[] getShortArray(@Nullable String key) {
        return mBundle.getShortArray(key);
    }

    @Nullable
    public char[] getCharArray(@Nullable String key) {
        return mBundle.getCharArray(key);
    }

    @Nullable
    public float[] getFloatArray(@Nullable String key) {
        return mBundle.getFloatArray(key);
    }

    @Nullable
    public CharSequence[] getCharSequenceArray(@Nullable String key) {
        return mBundle.getCharSequenceArray(key);
    }

    @Nullable
    public IBinder getBinder(@Nullable String key) {
        return mBundle.getBinder(key);
    }

    public static class Builder {

        private final int mAction;
        private final Bundle mBundle = new Bundle();

        private Builder(int action) {
            mAction = action;
        }

        public Builder putAll(Bundle bundle) {
             mBundle.putAll(bundle);
             return this;
        }

        public Builder putString(@Nullable String key, String value) {
            mBundle.putString(key, value);
            return this;
        }

        public Builder putBoolean(@Nullable String key, boolean value) {
            mBundle.putBoolean(key, value);
            return this;
        }

        public Builder putBooleanArray(@Nullable String key, boolean[] value) {
            mBundle.putBooleanArray(key, value);
            return this;
        }

        public Builder putByte(@Nullable String key, byte value) {
            mBundle.putByte(key, value);
            return this;
        }

        public Builder putChar(@Nullable String key, char value) {
            mBundle.putChar(key, value);
            return this;
        }

        public Builder putShort(@Nullable String key, short value) {
            mBundle.putShort(key, value);
            return this;
        }

        public Builder putFloat(@Nullable String key, float value) {
            mBundle.putFloat(key, value);
            return this;
        }

        public Builder putCharSequence(@Nullable String key, @Nullable CharSequence value) {
            mBundle.putCharSequence(key, value);
            return this;
        }

        public Builder putParcelable(@Nullable String key, @Nullable Parcelable value) {
            mBundle.putParcelable(key, value);
            return this;
        }

        public Builder putSize(@Nullable String key, @Nullable Size value) {
            mBundle.putSize(key, value);
            return this;
        }

        public Builder putSizeF(@Nullable String key, @Nullable SizeF value) {
            mBundle.putSizeF(key, value);
            return this;
        }

        public Builder putParcelableArray(@Nullable String key, @Nullable Parcelable[] value) {
            mBundle.putParcelableArray(key, value);
            return this;
        }

        public Builder putParcelableArrayList(@Nullable String key, @Nullable ArrayList<? extends Parcelable> value) {
            mBundle.putParcelableArrayList(key, value);
            return this;
        }

        public Builder putSparseParcelableArray(@Nullable String key, @Nullable SparseArray<? extends Parcelable> value) {
            mBundle.putSparseParcelableArray(key, value);
            return this;
        }

        public Builder putIntegerArrayList(@Nullable String key, @Nullable ArrayList<Integer> value) {
            mBundle.putIntegerArrayList(key, value);
            return this;
        }

        public Builder putStringArrayList(@Nullable String key, @Nullable ArrayList<String> value) {
            mBundle.putStringArrayList(key, value);
            return this;
        }

        public Builder putCharSequenceArrayList(@Nullable String key, @Nullable ArrayList<CharSequence> value) {
            mBundle.putCharSequenceArrayList(key, value);
            return this;
        }

        public Builder putSerializable(@Nullable String key, @Nullable Serializable value) {
            mBundle.putSerializable(key, value);
            return this;
        }

        public Builder putByteArray(@Nullable String key, @Nullable byte[] value) {
            mBundle.putByteArray(key, value);
            return this;
        }

        public Builder putShortArray(@Nullable String key, @Nullable short[] value) {
            mBundle.putShortArray(key, value);
            return this;
        }

        public Builder putCharArray(@Nullable String key, @Nullable char[] value) {
            mBundle.putCharArray(key, value);
            return this;
        }

        public Builder putFloatArray(@Nullable String key, @Nullable float[] value) {
            mBundle.putFloatArray(key, value);
            return this;
        }

        public Builder putCharSequenceArray(@Nullable String key, @Nullable CharSequence[] value) {
            mBundle.putCharSequenceArray(key, value);
            return this;
        }

        public Builder putBundle(@Nullable String key, @Nullable Bundle value) {
            mBundle.putBundle(key, value);
            return this;
        }

        public Builder putBinder(@Nullable String key, @Nullable IBinder value) {
            mBundle.putBinder(key, value);
            return this;
        }

        public void post() {
            ZBus.post(new BundleEvent(mAction, mBundle));
        }

    }

}
