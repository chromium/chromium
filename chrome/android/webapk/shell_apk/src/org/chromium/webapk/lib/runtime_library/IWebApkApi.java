/*
 * This file is auto-generated.  DO NOT MODIFY.
 */
package org.chromium.webapk.lib.runtime_library;

/** Interface for communicating between WebAPK service and Chrome. */
public interface IWebApkApi extends android.os.IInterface {
    /** Default implementation for IWebApkApi. */
    public static class Default implements org.chromium.webapk.lib.runtime_library.IWebApkApi {
        // Gets the id of the icon to represent WebAPK notifications in status bar.
        @Override
        public int getSmallIconId() throws android.os.RemoteException {
            return 0;
        }

        // Display a notification.
        // DEPRECATED: Use notifyNotificationWithChannel.
        @Override
        public void notifyNotification(
                java.lang.String platformTag, int platformID, android.app.Notification notification)
                throws android.os.RemoteException {}

        // Cancel a notification.
        @Override
        public void cancelNotification(java.lang.String platformTag, int platformID)
                throws android.os.RemoteException {}

        // Get if notification permission is enabled.
        // DEPRECATED: Use checkNotificationPermission instead.
        @Override
        public boolean notificationPermissionEnabled() throws android.os.RemoteException {
            return false;
        }

        // Display a notification with a specified channel name.
        @Override
        public void notifyNotificationWithChannel(
                java.lang.String platformTag,
                int platformID,
                android.app.Notification notification,
                java.lang.String channelName)
                throws android.os.RemoteException {}

        // Finishes and removes the WebAPK's task. Returns true on success.
        @Override
        public boolean finishAndRemoveTaskSdk23() throws android.os.RemoteException {
            return false;
        }

        // Gets the notification permission status.
        @Override
        public int checkNotificationPermission() throws android.os.RemoteException {
            return 0;
        }

        // Creates a pending intent for requesting notification permission.
        @Override
        public android.app.PendingIntent requestNotificationPermission(
                java.lang.String channelName, java.lang.String channelId)
                throws android.os.RemoteException {
            return null;
        }

        @Override
        public android.os.IBinder asBinder() {
            return null;
        }
    }

    /** Local-side IPC implementation stub class. */
    public abstract static class Stub extends android.os.Binder
            implements org.chromium.webapk.lib.runtime_library.IWebApkApi {
        /** Construct the stub at attach it to the interface. */
        public Stub() {
            this.attachInterface(this, DESCRIPTOR);
        }

        /**
         * Cast an IBinder object into an org.chromium.webapk.lib.runtime_library.IWebApkApi
         * interface, generating a proxy if needed.
         */
        public static org.chromium.webapk.lib.runtime_library.IWebApkApi asInterface(
                android.os.IBinder obj) {
            if ((obj == null)) {
                return null;
            }
            android.os.IInterface iin = obj.queryLocalInterface(DESCRIPTOR);
            if (((iin != null)
                    && (iin instanceof org.chromium.webapk.lib.runtime_library.IWebApkApi))) {
                return ((org.chromium.webapk.lib.runtime_library.IWebApkApi) iin);
            }
            return new org.chromium.webapk.lib.runtime_library.IWebApkApi.Stub.Proxy(obj);
        }

        @Override
        public android.os.IBinder asBinder() {
            return this;
        }

        @Override
        public boolean onTransact(
                int code, android.os.Parcel data, android.os.Parcel reply, int flags)
                throws android.os.RemoteException {
            java.lang.String descriptor = DESCRIPTOR;
            if (code >= android.os.IBinder.FIRST_CALL_TRANSACTION
                    && code <= android.os.IBinder.LAST_CALL_TRANSACTION) {
                data.enforceInterface(descriptor);
            }
            switch (code) {
                case INTERFACE_TRANSACTION:
                    reply.writeString(descriptor);
                    return true;
            }
            switch (code) {
                case TRANSACTION_getSmallIconId:
                    {
                        int _result = this.getSmallIconId();
                        reply.writeNoException();
                        reply.writeInt(_result);
                        break;
                    }
                case TRANSACTION_notifyNotification:
                    {
                        java.lang.String _arg0;
                        _arg0 = data.readString();
                        int _arg1;
                        _arg1 = data.readInt();
                        android.app.Notification _arg2;
                        _arg2 = _Parcel.readTypedObject(data, android.app.Notification.CREATOR);
                        this.notifyNotification(_arg0, _arg1, _arg2);
                        reply.writeNoException();
                        break;
                    }
                case TRANSACTION_cancelNotification:
                    {
                        java.lang.String _arg0;
                        _arg0 = data.readString();
                        int _arg1;
                        _arg1 = data.readInt();
                        this.cancelNotification(_arg0, _arg1);
                        reply.writeNoException();
                        break;
                    }
                case TRANSACTION_notificationPermissionEnabled:
                    {
                        boolean _result = this.notificationPermissionEnabled();
                        reply.writeNoException();
                        reply.writeInt(((_result) ? (1) : (0)));
                        break;
                    }
                case TRANSACTION_notifyNotificationWithChannel:
                    {
                        java.lang.String _arg0;
                        _arg0 = data.readString();
                        int _arg1;
                        _arg1 = data.readInt();
                        android.app.Notification _arg2;
                        _arg2 = _Parcel.readTypedObject(data, android.app.Notification.CREATOR);
                        java.lang.String _arg3;
                        _arg3 = data.readString();
                        this.notifyNotificationWithChannel(_arg0, _arg1, _arg2, _arg3);
                        reply.writeNoException();
                        break;
                    }
                case TRANSACTION_finishAndRemoveTaskSdk23:
                    {
                        boolean _result = this.finishAndRemoveTaskSdk23();
                        reply.writeNoException();
                        reply.writeInt(((_result) ? (1) : (0)));
                        break;
                    }
                case TRANSACTION_checkNotificationPermission:
                    {
                        int _result = this.checkNotificationPermission();
                        reply.writeNoException();
                        reply.writeInt(_result);
                        break;
                    }
                case TRANSACTION_requestNotificationPermission:
                    {
                        java.lang.String _arg0;
                        _arg0 = data.readString();
                        java.lang.String _arg1;
                        _arg1 = data.readString();
                        android.app.PendingIntent _result =
                                this.requestNotificationPermission(_arg0, _arg1);
                        reply.writeNoException();
                        _Parcel.writeTypedObject(
                                reply,
                                _result,
                                android.os.Parcelable.PARCELABLE_WRITE_RETURN_VALUE);
                        break;
                    }
                default:
                    {
                        return super.onTransact(code, data, reply, flags);
                    }
            }
            return true;
        }

        private static class Proxy implements org.chromium.webapk.lib.runtime_library.IWebApkApi {
            private android.os.IBinder mRemote;

            Proxy(android.os.IBinder remote) {
                mRemote = remote;
            }

            @Override
            public android.os.IBinder asBinder() {
                return mRemote;
            }

            public java.lang.String getInterfaceDescriptor() {
                return DESCRIPTOR;
            }

            // Gets the id of the icon to represent WebAPK notifications in status bar.
            @Override
            public int getSmallIconId() throws android.os.RemoteException {
                android.os.Parcel _data = android.os.Parcel.obtain();
                android.os.Parcel _reply = android.os.Parcel.obtain();
                int _result;
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    boolean _status =
                            mRemote.transact(Stub.TRANSACTION_getSmallIconId, _data, _reply, 0);
                    _reply.readException();
                    _result = _reply.readInt();
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
                return _result;
            }

            // Display a notification.
            // DEPRECATED: Use notifyNotificationWithChannel.
            @Override
            public void notifyNotification(
                    java.lang.String platformTag,
                    int platformID,
                    android.app.Notification notification)
                    throws android.os.RemoteException {
                android.os.Parcel _data = android.os.Parcel.obtain();
                android.os.Parcel _reply = android.os.Parcel.obtain();
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    _data.writeString(platformTag);
                    _data.writeInt(platformID);
                    _Parcel.writeTypedObject(_data, notification, 0);
                    boolean _status =
                            mRemote.transact(Stub.TRANSACTION_notifyNotification, _data, _reply, 0);
                    _reply.readException();
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
            }

            // Cancel a notification.
            @Override
            public void cancelNotification(java.lang.String platformTag, int platformID)
                    throws android.os.RemoteException {
                android.os.Parcel _data = android.os.Parcel.obtain();
                android.os.Parcel _reply = android.os.Parcel.obtain();
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    _data.writeString(platformTag);
                    _data.writeInt(platformID);
                    boolean _status =
                            mRemote.transact(Stub.TRANSACTION_cancelNotification, _data, _reply, 0);
                    _reply.readException();
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
            }

            // Get if notification permission is enabled.
            // DEPRECATED: Use checkNotificationPermission instead.
            @Override
            public boolean notificationPermissionEnabled() throws android.os.RemoteException {
                android.os.Parcel _data = android.os.Parcel.obtain();
                android.os.Parcel _reply = android.os.Parcel.obtain();
                boolean _result;
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    boolean _status =
                            mRemote.transact(
                                    Stub.TRANSACTION_notificationPermissionEnabled,
                                    _data,
                                    _reply,
                                    0);
                    _reply.readException();
                    _result = (0 != _reply.readInt());
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
                return _result;
            }

            // Display a notification with a specified channel name.
            @Override
            public void notifyNotificationWithChannel(
                    java.lang.String platformTag,
                    int platformID,
                    android.app.Notification notification,
                    java.lang.String channelName)
                    throws android.os.RemoteException {
                android.os.Parcel _data = android.os.Parcel.obtain();
                android.os.Parcel _reply = android.os.Parcel.obtain();
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    _data.writeString(platformTag);
                    _data.writeInt(platformID);
                    _Parcel.writeTypedObject(_data, notification, 0);
                    _data.writeString(channelName);
                    boolean _status =
                            mRemote.transact(
                                    Stub.TRANSACTION_notifyNotificationWithChannel,
                                    _data,
                                    _reply,
                                    0);
                    _reply.readException();
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
            }

            // Finishes and removes the WebAPK's task. Returns true on success.
            @Override
            public boolean finishAndRemoveTaskSdk23() throws android.os.RemoteException {
                android.os.Parcel _data = android.os.Parcel.obtain();
                android.os.Parcel _reply = android.os.Parcel.obtain();
                boolean _result;
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    boolean _status =
                            mRemote.transact(
                                    Stub.TRANSACTION_finishAndRemoveTaskSdk23, _data, _reply, 0);
                    _reply.readException();
                    _result = (0 != _reply.readInt());
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
                return _result;
            }

            // Gets the notification permission status.
            @Override
            public int checkNotificationPermission() throws android.os.RemoteException {
                android.os.Parcel _data = android.os.Parcel.obtain();
                android.os.Parcel _reply = android.os.Parcel.obtain();
                int _result;
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    boolean _status =
                            mRemote.transact(
                                    Stub.TRANSACTION_checkNotificationPermission, _data, _reply, 0);
                    _reply.readException();
                    _result = _reply.readInt();
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
                return _result;
            }

            // Creates a pending intent for requesting notification permission.
            @Override
            public android.app.PendingIntent requestNotificationPermission(
                    java.lang.String channelName, java.lang.String channelId)
                    throws android.os.RemoteException {
                android.os.Parcel _data = android.os.Parcel.obtain();
                android.os.Parcel _reply = android.os.Parcel.obtain();
                android.app.PendingIntent _result;
                try {
                    _data.writeInterfaceToken(DESCRIPTOR);
                    _data.writeString(channelName);
                    _data.writeString(channelId);
                    boolean _status =
                            mRemote.transact(
                                    Stub.TRANSACTION_requestNotificationPermission,
                                    _data,
                                    _reply,
                                    0);
                    _reply.readException();
                    _result = _Parcel.readTypedObject(_reply, android.app.PendingIntent.CREATOR);
                } finally {
                    _reply.recycle();
                    _data.recycle();
                }
                return _result;
            }
        }

        static final int TRANSACTION_getSmallIconId =
                (android.os.IBinder.FIRST_CALL_TRANSACTION + 0);
        static final int TRANSACTION_notifyNotification =
                (android.os.IBinder.FIRST_CALL_TRANSACTION + 1);
        static final int TRANSACTION_cancelNotification =
                (android.os.IBinder.FIRST_CALL_TRANSACTION + 2);
        static final int TRANSACTION_notificationPermissionEnabled =
                (android.os.IBinder.FIRST_CALL_TRANSACTION + 3);
        static final int TRANSACTION_notifyNotificationWithChannel =
                (android.os.IBinder.FIRST_CALL_TRANSACTION + 4);
        static final int TRANSACTION_finishAndRemoveTaskSdk23 =
                (android.os.IBinder.FIRST_CALL_TRANSACTION + 5);
        static final int TRANSACTION_checkNotificationPermission =
                (android.os.IBinder.FIRST_CALL_TRANSACTION + 6);
        static final int TRANSACTION_requestNotificationPermission =
                (android.os.IBinder.FIRST_CALL_TRANSACTION + 7);
    }

    public static final java.lang.String DESCRIPTOR =
            "org.chromium.webapk.lib.runtime_library.IWebApkApi";

    // Gets the id of the icon to represent WebAPK notifications in status bar.
    public int getSmallIconId() throws android.os.RemoteException;

    // Display a notification.
    // DEPRECATED: Use notifyNotificationWithChannel.
    public void notifyNotification(
            java.lang.String platformTag, int platformID, android.app.Notification notification)
            throws android.os.RemoteException;

    // Cancel a notification.
    public void cancelNotification(java.lang.String platformTag, int platformID)
            throws android.os.RemoteException;

    // Get if notification permission is enabled.
    // DEPRECATED: Use checkNotificationPermission instead.
    public boolean notificationPermissionEnabled() throws android.os.RemoteException;

    // Display a notification with a specified channel name.
    public void notifyNotificationWithChannel(
            java.lang.String platformTag,
            int platformID,
            android.app.Notification notification,
            java.lang.String channelName)
            throws android.os.RemoteException;

    // Finishes and removes the WebAPK's task. Returns true on success.
    public boolean finishAndRemoveTaskSdk23() throws android.os.RemoteException;

    // Gets the notification permission status.
    public int checkNotificationPermission() throws android.os.RemoteException;

    // Creates a pending intent for requesting notification permission.
    public android.app.PendingIntent requestNotificationPermission(
            java.lang.String channelName, java.lang.String channelId)
            throws android.os.RemoteException;

    /** @hide */
    static class _Parcel {
        private static <T> T readTypedObject(
                android.os.Parcel parcel, android.os.Parcelable.Creator<T> c) {
            if (parcel.readInt() != 0) {
                return c.createFromParcel(parcel);
            } else {
                return null;
            }
        }

        private static <T extends android.os.Parcelable> void writeTypedObject(
                android.os.Parcel parcel, T value, int parcelableFlags) {
            if (value != null) {
                parcel.writeInt(1);
                value.writeToParcel(parcel, parcelableFlags);
            } else {
                parcel.writeInt(0);
            }
        }
    }
}
