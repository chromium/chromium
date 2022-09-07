// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.os.Parcel;
import android.os.ParcelFileDescriptor;
import android.os.Parcelable;

import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.UsedByReflection;

import javax.annotation.concurrent.Immutable;

/**
 * Parcelable class that contains file descriptor and file region information to
 * be passed to child processes.
 */
@Immutable
@MainDex
@UsedByReflection("child_process_launcher_helper_android.cc")
public final class FileDescriptorInfo implements Parcelable {
    public final int id;
    public final ParcelFileDescriptor fd;
    public final long offset;
    public final long size;

    public FileDescriptorInfo(int id, ParcelFileDescriptor fd, long offset, long size) {
        this.id = id;
        this.fd = fd;
        this.offset = offset;
        this.size = size;
    }

    FileDescriptorInfo(Parcel in) {
        id = in.readInt();
        fd = in.readParcelable(ParcelFileDescriptor.class.getClassLoader());
        offset = in.readLong();
        size = in.readLong();
    }

    @Override
    public int describeContents() {
        return CONTENTS_FILE_DESCRIPTOR;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeInt(id);
        dest.writeParcelable(fd, CONTENTS_FILE_DESCRIPTOR);
        dest.writeLong(offset);
        dest.writeLong(size);
    }

    public static final Parcelable.Creator<FileDescriptorInfo> CREATOR =
            new Parcelable.Creator<FileDescriptorInfo>() {
                @Override
                public FileDescriptorInfo createFromParcel(Parcel in) {
                    return new FileDescriptorInfo(in);
                }

                @Override
                public FileDescriptorInfo[] newArray(int size) {
                    return new FileDescriptorInfo[size];
                }
            };
}
