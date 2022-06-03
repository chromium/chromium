# Forwarding and `ReadWriteLock`

This document explains the approach chosen to implement
[forwarding](https://en.wikipedia.org/wiki/Forwarding_(object-oriented_programming))
protected with a `ReadWriteLock`.

A `ReadWriteLock` is used rather than `synchronized` blocks to the limit
opportunitie for stutter on the UI thread when waiting for this shared resource.

For context see
`base/android/java/src/org/chromium/base/metrics/CachingUmaRecorder.java`

### Method A

```java
private final ReentrantReadWriteLock mRwLock = new ReentrantReadWriteLock();

@Nullable
private UmaRecorder mDelegate;

@Override
public void record(Sample sample) {
    mRwLock.readLock().lock();
    try {
        if (mDelegate != null) {
            mDelegate.record(sample);  // Called with a read lock
            return;
        }
    } finally {
        mRwLock.readLock().unlock();
    }

    mRwLock.writeLock().lock();
    try {
        if (mDelegate == null) {
            this.cache(sample);
            return; // Skip the lock downgrade.
        }
        // Downgrade by acquiring read lock before releasing write lock
        mRwLock.readLock().lock();
    } finally {
        mRwLock.writeLock().unlock();
    }

    // Downgraded to read lock.
    try {
        assert mDelegate != null;
        mDelegate.record(sample);  // Called with a read lock
    } finally {
        mRwLock.readLock().unlock();
    }
}
```

### Method B

```java
private final ReentrantReadWriteLock mRwLock = new ReentrantReadWriteLock();

@Nullable
private UmaRecorder mDelegate;

@Override
public void record(Sample sample) {
    mRwLock.readLock().lock();
    try {
        if (mDelegate != null) {
            mDelegate.record(sample);  // Called with a read lock
            return;
        }
    } finally {
        mRwLock.readLock().unlock();
    }

    mRwLock.writeLock().lock();
    try {
        if (mDelegate == null) {
            this.cache(sample);
        } else {
            mDelegate.record(sample);  // Called with a *write* lock
        }
    } finally {
        mRwLock.writeLock().unlock();
    }
}
```

## Reasoning

Code of method B is visibly and conceptually simpler, since it doesn't involve
lock downgrading. However:

 *  Method B invokes `record(Sample)` with one of two available locks. The two
    locks are difficult to distinguish in a stack trace.
 *  Method A always uses the read lock which results in a less complex
    interaction with `mDelegate`.

Even if we ask every implementation of `UmaRecord#record(Sample)` to not block
and run quickly, bugs happen.

### Hypothetical example

An invocation of `UmaRecord#record(Sample)` under some circumstances interacts
with the same instance of `CachingUmaRecorder`. We don't want this to ever
happen, so this is a bug. The current delegate implementation crosses JNI and
calls into metrics code that should terminate without crossing back to Java.

This bug results in the following scenarios:

| # | `CUR` holds  | `mDelegate` uses | thread    | result   |
|---|--------------|------------------|-----------|----------|
| 1 | `readLock`   | `readLock`       | same      | OK       |
| 2 | `readLock`   | `writeLock`      | same      | deadlock |
| 3 | `readLock`   | `readLock`       | different | OK       |
| 4 | `readLock`   | `writeLock`      | different | blocks   |
| 5 | `writeLock`  | `readLock`       | same      | OK       |
| 6 | `writeLock`  | `writeLock`      | same      | OK       |
| 7 | `writeLock`  | `readLock`       | different | blocks   |
| 8 | `writeLock`  | `writeLock`      | different | blocks   |

Method A eliminates the possibility of scenarios 5-8.
