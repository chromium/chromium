# ScopedProfileKeepAlive

`ScopedProfileKeepAlive` is a strong reference to a `Profile` object, which is
refcounted when the DestroyProfileOnBrowserClose flag is enabled. It is very
similar to `ScopedKeepAlive`, which is for the browser process.
