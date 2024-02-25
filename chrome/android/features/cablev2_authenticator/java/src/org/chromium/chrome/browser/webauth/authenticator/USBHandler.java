// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

import android.content.Context;
import android.hardware.usb.UsbAccessory;
import android.hardware.usb.UsbManager;
import android.os.ParcelFileDescriptor;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import android.system.StructPollfd;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SingleThreadTaskRunner;
import org.chromium.base.task.TaskTraits;

import java.io.Closeable;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;

// USBHandler implements I/O and basic message framing for carrying CTAP2 over the Android Accessory
// protocol[1]. It forms a counterpart to the implementation in //device/fido/aoa. It is intended to
// be used with the Intent-based flow[2] for getting a handle to a {@link UsbAccessory}.
//
// [1] https://source.android.com/devices/accessories/aoa
// [2] https://developer.android.com/guide/topics/connectivity/usb/accessory#discover-a-intent
class USBHandler implements Closeable {
    // These two values must match up with the values in
    // android_accessory_device.h.
    private static final byte COAOA_SYNC = 119;
    private static final byte COAOA_MSG = 33;

    // These values must, implicitly, match the other implementation in
    // android_accessory_device.cc.
    private static final int SYNC_LENGTH = 17;
    private static final int MSG_HEADER_LENGTH = 5;

    private static final String TAG = "CableUSBHandler";

    private final UsbAccessory mAccessory;
    private final Context mContext;
    private final SingleThreadTaskRunner mTaskRunner;
    private final UsbManager mUsbManager;
    private final StructPollfd[] mPollFds;

    private ParcelFileDescriptor mFd;
    private FileInputStream mInput;
    private FileOutputStream mOutput;
    // mStopped should only be accessed via the synchronized functions
    // |haveStopped| and |setStopped| because it spans threads. See the comment
    // in |read| for motivation.
    private boolean mStopped;

    private byte[] mBuffer;
    private int mBufferUsed;
    private int mBufferOffset;

    USBHandler(Context context, SingleThreadTaskRunner taskRunner, UsbAccessory accessory) {
        mAccessory = accessory;
        mContext = context;
        mTaskRunner = taskRunner;
        mUsbManager = (UsbManager) context.getSystemService(Context.USB_SERVICE);
        mPollFds = new StructPollfd[1];
        mPollFds[0] = new StructPollfd();
    }

    @CalledByNative
    public void startReading() {
        assert mTaskRunner.belongsToCurrentThread();
        openAccessory(mAccessory);
    }

    @Override
    @CalledByNative
    public void close() {
        assert mTaskRunner.belongsToCurrentThread();

        setStopped();

        if (mFd != null) {
            try {
                mFd.close();
            } catch (IOException e) {
            }
        }
    }

    /**
     * Called by CableAuthenticator to write a deferred reply (e.g. to a makeCredential or
     * getAssertion request).
     */
    @CalledByNative
    public void write(byte[] message) {
        assert mTaskRunner.belongsToCurrentThread();
        assert mOutput != null;

        doWrite(message);
    }

    private synchronized boolean haveStopped() {
        return mStopped;
    }

    private synchronized void setStopped() {
        mStopped = true;
    }

    private void openAccessory(UsbAccessory accessory) {
        assert mTaskRunner.belongsToCurrentThread();

        if (haveStopped()) {
            return;
        }

        mFd = mUsbManager.openAccessory(accessory);
        Log.i(TAG, "Accessory opened " + accessory);
        if (mFd == null) {
            Log.i(TAG, "Returned file descriptor is null");
            USBHandlerJni.get().onUSBData(null);
            return;
        }

        FileDescriptor fd = mFd.getFileDescriptor();
        mInput = new FileInputStream(fd);
        mOutput = new FileOutputStream(fd);

        // The Android documentation[1] suggests that reads with too small a
        // buffer will discard the extra like a datagram socket:
        //   > When reading [...] ensure that the buffer that you use is big enough to store the USB
        //   > packet data. The Android accessory protocol supports packet buffers up to 16384
        //   > bytes, so you can choose to always declare your buffer to be of this size for
        //   > simplicity.
        // The kernel source doesn't actually appear to do that but, in case that changes, a 16KiB
        // buffer is used and the usual, incremental, read operation is built on top of it.
        mBuffer = new byte[16384];
        mBufferUsed = 0;
        mBufferOffset = 0;

        PostTask.postTask(
                TaskTraits.BEST_EFFORT_MAY_BLOCK,
                () -> {
                    this.readLoop();
                });
    }

    /**
     * Implements a standard, incremental, read operation on top of the kernel's read operation,
     * which discards any data that doesn't fit into the provided buffer. Returns the number of
     * bytes read, or -1 on error.
     */
    private int read(byte[] buffer, int offset, int len) throws IOException {
        while (mBufferUsed == mBufferOffset) {
            // Refill the buffer so that there's some data. Android has a bug where closing an
            // accessory file descriptor does not unblock pending reads. Thus doing a simple read(2)
            // doesn't work because it'll end up stuck forever. Thus the file descriptor is polled
            // with a timeout and, each time the timeout triggers, a flag is inspected to see
            // whether the descriptor has been closed.
            mPollFds[0].fd = mFd.getFileDescriptor();
            mPollFds[0].events = (short) OsConstants.POLLIN;
            while (true) {
                int pollRet;
                try {
                    pollRet = Os.poll(mPollFds, 200);
                } catch (ErrnoException e) {
                    pollRet = -1;
                }

                if (pollRet < 0) {
                    return pollRet;
                } else if (pollRet == 0) {
                    // Timeout.
                    if (haveStopped()) {
                        return -1;
                    }
                    continue;
                }

                assert pollRet == 1;
                int n = mInput.read(mBuffer, 0, mBuffer.length);
                if (n <= 0) {
                    return -1;
                }
                mBufferUsed = n;
                mBufferOffset = 0;
                break;
            }
        }

        // Some data exists in the internal buffer. Return as much as we can.
        int todo = mBufferUsed - mBufferOffset;
        if (todo > len) {
            todo = len;
        }
        System.arraycopy(mBuffer, mBufferOffset, buffer, offset, todo);
        mBufferOffset += todo;
        return todo;
    }

    /** Utility function that builds on read() to completely fill a buffer */
    private boolean readAll(byte[] buffer) {
        int done = 0;
        while (done < buffer.length) {
            int n;
            try {
                n = read(buffer, done, buffer.length - done);
            } catch (IOException e) {
                return false;
            }
            if (n < 0) {
                return false;
            }
            done += n;
        }

        return true;
    }

    /* Reads a non-negative int32 from the given offset. */
    private static int getNonNegativeS32(byte[] message, int offset) {
        return (((int) message[offset + 0]) & 0xff)
                | ((((int) message[offset + 1]) & 0xff) << 8)
                | ((((int) message[offset + 2]) & 0xff) << 16)
                | ((((int) message[offset + 3]) & 0x7f) << 24);
    }

    /**
     * Reads messages from the USB peer forever. This consumes a thread in the thread-pool, which is
     * a little rude, but the browser isn't doing anything else while it's doing security key
     * operations. It'll exit when it hits an error which, if nothing else, will be triggered when
     * close() sets |mStopped|.
     */
    private void readLoopInner() {
        byte[] syncMessage = new byte[SYNC_LENGTH];
        byte[] msgHeader = new byte[MSG_HEADER_LENGTH];
        byte[] restOfSyncMessage = null;

        // It is possible for several transactions to occur over a USB accessory
        // connection. However, if the peer canceled an operation and stopped
        // reading, that operation may still have completed on the phone.
        // Therefore a stray reply might be sitting waiting to confuse a future
        // operation from the desktop. Desktops thus send a synchronisation
        // message containing a random nonce and discard data until the matching
        // synchronisation reply is found.
        if (!readAll(syncMessage)) {
            return;
        }

        for (; ; ) {
            // The next message must be a synchronisation message, either
            // because it's the first message, or because the loop below
            // encountered a message type other than |COAOA_MSG| and there's
            // only two valid message types.
            if (syncMessage[0] != COAOA_SYNC) {
                Log.i(TAG, "Found unexpected message type");
                return;
            }

            // Echo the synchronisation message (which contains a random nonce)
            // back to the peer so that it knows where the replies to its
            // messages begin.
            try {
                mOutput.write(syncMessage);
            } catch (IOException e) {
                Log.i(TAG, "Failed to write sync message");
                return;
            }

            // Read messages until EOF or desync.
            for (; ; ) {
                if (!readAll(msgHeader)) {
                    return;
                }
                if (msgHeader[0] != COAOA_MSG) {
                    if (restOfSyncMessage == null) {
                        restOfSyncMessage = new byte[SYNC_LENGTH - MSG_HEADER_LENGTH];
                    }
                    if (!readAll(restOfSyncMessage)) {
                        return;
                    }
                    System.arraycopy(msgHeader, 0, syncMessage, 0, msgHeader.length);
                    System.arraycopy(
                            restOfSyncMessage,
                            0,
                            syncMessage,
                            msgHeader.length,
                            restOfSyncMessage.length);
                    break;
                }

                int length = getNonNegativeS32(msgHeader, 1);
                // Enforce 1MB sanity limit on messages.
                if (length > (1 << 20)) {
                    Log.i(TAG, "Message too long");
                    return;
                }
                byte[] message = new byte[length];
                if (!readAll(message)) {
                    return;
                }
                mTaskRunner.postTask(() -> this.didRead(message));
            }
        }
    }

    /**
     * Wrap {@link readLoopInner} so that error paths can simply |return| rather than having to
     * remember to send a null message in every case.
     */
    private void readLoop() {
        readLoopInner();
        Log.i(TAG, "Read loop has exited.");
        mTaskRunner.postTask(() -> this.didRead(null));
    }

    /** Called with each message read from USB, or null on transport error. */
    private void didRead(byte[] buffer) {
        assert mTaskRunner.belongsToCurrentThread();

        if (haveStopped()) {
            return;
        }

        if (buffer == null) {
            Log.i(TAG, "Error reading from USB");
        }

        USBHandlerJni.get().onUSBData(buffer);
    }

    private void doWrite(byte[] buffer) {
        byte[] headerBytes = new byte[5];
        headerBytes[0] = COAOA_MSG;
        headerBytes[1] = (byte) buffer.length;
        headerBytes[2] = (byte) (buffer.length >>> 8);
        headerBytes[3] = (byte) (buffer.length >>> 16);
        headerBytes[4] = (byte) (buffer.length >>> 24);

        try {
            mOutput.write(headerBytes);
            mOutput.write(buffer);
        } catch (IOException e) {
            // It's assumed that any errors will be caught by the reading thread.
            Log.i(TAG, "USB write failed");
        }
    }

    @NativeMethods("cablev2_authenticator")
    interface Natives {
        // onUSBData is called when data is read from the USB data. If data is
        // null then an error occurred.
        void onUSBData(byte[] data);
    }
}
