# Fusebox

Fusebox is a ChromeOS-only mechanism for exposing Chrome's in-process virtual
file system (its [`storage` C++
API](https://source.chromium.org/chromium/chromium/src/+/main:storage/browser/file_system/))
on the 'real' (kernel-level) virtual file system, via [Linux's
FUSE](https://www.kernel.org/doc/html/latest/filesystems/fuse.html) protocol.

It enables sharing virtual-file-like things *across processes* (e.g. between
ash-chrome and lacros-chrome) or *with Virtual Machines* (e.g. the Android or
Crostini VMs) just by sharing a string file name or an integer file descriptor.

Fusebox doesn't *replace* the `storage` C++ API. It provides *an alternative
mechanism* for accessing those virtual files. Workflows that stay entirely
within ash-chrome can continue to use the C++ API. But when the GMail web-app
(running in a sandboxed lacros-chrome process) wants to upload files from a
phone attached to a Chromebook via USB cable, and the MTP (Media Transfer
Protocol) volume (virtual directory) is served by ash-chrome code, that access
is facilitated by Fusebox.


## Structure

There are multiple processes involved. The two key ones communicate over D-Bus:

- ash-chrome is the D-Bus server, also known as the Fusebox Server. This
  process does not speak FUSE per se.
- `/usr/bin/fusebox` is the D-Bus client, also known as the Fusebox Client or
  the Fusebox Daemon. This is the process that speaks FUSE with the kernel.
  Like other FUSE daemons on ChromeOS, this is managed by
  [`cros-disks`](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform2/cros-disks/)
  and runs in a [minijail
  sandbox](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform2/cros-disks/seccomp_policy).

Those are the two key processes. More processes are involved when an arbitrary
process (e.g. lacros-chrome or another process running in an Android VM) wants
to read a Fusebox file:

```
arbitrary-proc  <-libc->  kernel  <-FUSE->  FuseboxDaemon  <-D-Bus->  ash-chrome
```

Specifically when "an arbitrary process" is lacros-chrome, we could skip some
hops with a [direct
connection](https://chromium.googlesource.com/chromium/src.git/+/main/chromeos/crosapi)
between lacros-chrome and ash-chrome. But that optimization is not implemented
yet (as of March 2023).

`cros-disks` forks/execs the Fusebox Daemon at user log-in. But after start-up,
`cros-disks` is not involved in Fusebox serving virtual files.


### D-Bus

The Fusebox Server has some bookkeeping code because D-Bus RPCs are "1 request,
1 response" but some `storage` C++ API calls are "1 request, multiple
(streaming) responses". In Fusebox's D-Bus protocol, the `cookie` is the common
numeric identifier that groups these request/response pairs.

We may move our IPC system from D-Bus to Mojo in the future, for this and other
reasons, especially as we don't really use D-Bus' structured types anymore
(they're hard to evolve, since the client and server live in different source
code repositories). Fusebox only uses D-Bus as a simple pipe for flinging
[Fusebox-specific
protobufs](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform2/system_api/dbus/fusebox/fusebox.proto)
around. But for now (March 2023), it's D-Bus.

The method names on Fusebox' D-Bus interface (e.g. "Open", "Read", "Write",
etc.) typically correspond 1:1 with both FUSE methods and Chrome's `storage`
C++ API methods, although those two systems sometimes use different names (e.g.
"Unlink" and "Rmdir" versus "RemoveFile" and "RemoveDirectory").

Some method names have a "2" suffix, "Read2" versus "Read", because the
original version used D-Bus' structured types as arguments. As above, these are
hard to evolve (e.g. add a new field) without atomic cross-repository commits.
The "2" versions speak protobufs-over-D-Bus instead.


## File Names

Fusebox file names (on the kernel-visible file system) look like
`/media/fuse/fusebox/abc.1234/foo/bar.txt`. The `abc.1234` is also called the
Fusebox Subdir (or just the Subdir), as a single Fusebox daemon process can
serve multiple volumes.

The `abc` part of the Subdir identifies the volume type:

- `adp` = Android Documents Provider, an Android (Java) API. For example,
  Dropbox has an official Android app, which can run on Chromebooks, making
  someone's Dropbox folder-in-the-cloud appear in the ChromeOS Files App.
- `fsp` = File System Provider, a Chrome (JavaScript) API. For example, Chrome
  extensions can implement virtual file systems.
- `mtp` = Media Transfer Protocol, via ChromeOS' system-global [platform2/mtpd
  daemon](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform2/mtpd).
  For example, phones and tablets that are attached to a Chromebook via USB
  cable and have opted in to sharing their files.
- `tmp` = Temporary filesystem (a subdirectory of `/tmp`), for testing.

The `1234` part of the Subdir, typically a base-64 encoded hash code,
identifies different volumes of that type. For example, somebody could mount
multiple ADP volumes, and they'd get different `adp.*` Subdirs. These hashes
(and hence file names) aim to be stable for what's conceptually "the same
volume". For example, unplugging a phone from a USB port and plugging the same
phone into a different port shouldn't change the Subdir.

The `foo/bar.txt` part is the relative path within the volume root. For
example, `Download/cake.jpg` could identify a photo in an attached phone's
`Download` directory.


### Built-In File Names

The Fusebox Client also serves some files under `/media/fuse/fusebox/built_in`,
mainly for basic debugging. For example, some of these `built_in` files may
still be informative even when the Fusebox Client cannot connect to the Fusebox
Server.


## Source code

The Fusebox Server (Chrome) code primarily lives in this directory,
`chrome/browser/ash/fusebox`. Unsurprisingly, `fusebox_server.cc` is the
centerpiece. Part of its code is bureaucracy because D-Bus code and callbacks
run on the main (UI) thread while `storage` code and callbacks run on the IO
thread. Blocking I/O belongs on yet another thread (or a pool of worker
threads).

A little bit of Fusebox-specific D-Bus bureaucracy lives in the
`fusebox_service_provider.*` files in a sibling directory,
`chrome/browser/ash/dbus`.

Fusebox integration with the Files App (ChromeOS' graphical file manager), via
its "under the hood" Volume Manager, and related "talk to `cros-disks`" code
lives in another sibling directory, `chrome/browser/ash/file_manager`.

The Fusebox Client code lives in the [`fusebox`
directory](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform2/fusebox/)
in the `platform2` repository.


### `storage` C++ API

The Fusebox Server layers over Chrome's `storage` C++ API. The interface's
source code lives in the `storage/browser/file_system` directory and backing
implementations are elsewhere. For example:

- the ADP implementation is in `chrome/browser/ash/arc/fileapi`.
- the FSP implementation is in `chrome/browser/ash/file_system_provider/fileapi`.
- the MTP implementation is in `chrome/browser/media_galleries/fileapi`.
- the 'real' file system implementation is in `storage/browser/file_system`.

`storage` was historically designed around serving a [cross-browser JS
API](https://developer.mozilla.org/en-US/docs/Web/API/FileSystem), allowing
multiple, independent web apps (each running untrusted code) to access
persistent storage without interfering with each other. Fusebox uses it (e.g.
[`storage::FileSystemURL`](https://source.chromium.org/chromium/chromium/src/+/main:storage/browser/file_system/file_system_url.h))
largely because that's how Chrome's 'Virtual File Systems' are implemented.
Some `storage::FileSystemURL` concepts such as their `url::Origin` and
`blink::StorageKey` are core to the API but less relevant for Fusebox's use.


## FUSE Handles

When the kernel sends the FUSE server an "open" request (and a string path),
the response contains a numeric FUSE handle (sometimes abbreviated as `fh`,
just like how a numeric file descriptor can be `fd`). Subsequent "read"
requests contain the handle but not the path.

These FUSE Handle numbers are similar to inode numbers, in that they're
server-defined and opaque to the client, but they are not the same. Just as the
one file can be opened multiple times, the one inode can be associated with
multiple file descriptors (on the 'file system client' side) and multiple FUSE
handles (on the 'file system server' side).


## Monikers

Monikers are a Fusebox concept (but not a FUSE concept). They are similar to
symlinks, in that they are an alternative name to an existing thing. They are
unlike symlinks in that the link target *does not otherwise exist* on the
'real' file system. The link target is a `storage::FileSystemURL`.

Fusebox Monikers are used for ad-hoc sharing on demand, typically for
individual files instead of directories or volumes. They are for "share this
one (virtual) file with only this one app" rather than "make this
folder-in-the-cloud available as an ambient collection of (virtual) files".

Moniker file names look like `/media/fuse/fusebox/moniker/123etc789`. `moniker`
is the entire Subdir and the `123etc789` is an unguessable random number.

See the `fusebox_moniker.h` comments for more detail.


## Testing

As the interesting parts of Fusebox involve multiple processes (Fusebox Client,
Fusebox Server and more), we rely more on integration tests (tast) than unit
tests. The test code lives in the `platform` repository, under
[`tast-tests/src/chromiumos/tast/local/bundles/cros/filemanager/`](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/chromiumos/tast/local/bundles/cros/filemanager/).


## More Information

Here are some Google-internal slide decks (the speaker notes also link to video
recordings):

- [Fusebox Overview](https://goto.google.com/fusebox-deck-2022), October 2022.
- [Fusebox Code
  Walkthrough](https://goto.google.com/fusebox-code-walkthrough-2023), February
  2023.
- [How to Build ChromiumOS'
  Fusebox](https://goto.google.com/how-to-build-cros-fusebox), January 2024.

There's also [the ChromeOS Files Team site](https://goto.google.com/xf-site)
for more general information. It is also Google-internal.
