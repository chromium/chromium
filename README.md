
This repository is a chromium fork that has has been adapted to use the record/replay driver.

# Setting up builds

Only one build configuration is currently supported.

Install chromium tree and depot_tools per https://www.chromium.org/developers/how-tos/get-the-code.

Then change update the remote URLs for chromium and V8 to our forks.  The `gclient sync` here ensures that other third party dependencies are at the right point for our chromium fork, and not upstream tip.

```
cd /path/to/src
git remote set-url origin https://github.com/replayio/chromium.git
git branch -D main
git pull
git checkout master
gclient sync
cd /path/to/src/v8
git remote set-url origin https://github.com/replayio/chromium-v8.git
git branch -D master
git pull
git checkout master
cd /path/to/src/third_party/webrtc
git remote set-url origin https://github.com/replayio/chromium-webrtc.git
git branch -D master
git pull
git checkout main
cd /path/to/src/third_party/skia
git remote set-url origin https://github.com/replayio/chromium-skia.git
git branch -D master
git pull
git checkout main
```

Setup engflow:

```
export GOMA_SERVER_HOST=simpsonite.goma.engflow.com
export GOMACTL_USE_PROXY=false
goma_auth login
goma_ctl restart
```

Setup the build:

```
cd /path/to/src
gn gen out/Release
gn args out/Release
```

Add the following settings:

```
use_goma = true # if using engflow, false if otherwise
is_debug = false
enable_nacl = false
```

On linux add the following settings to disable the allocator shim. This may not be
necessary but avoids interactions between the driver and chromium.

```
use_allocator = "none"
use_allocator_shim = false
```

On linux the following setting may be helpful for looking at stack traces while
recording. This isn't used for production builds because the resulting binary size
is pretty huge.

```
symbol_level = 1
```

Build:

```
node build
```

# Merging from upstream

Because chromium's source is split across many git repositories, merging changes from upstream is tricky

Pull upstream changes and merge into master branch:

```
git checkout upstream
git pull https://github.com/chromium/chromium.git master
git push
git checkout master
git merge upstream
... fix merge conflicts ...
git commit -a
git push
```

Update other dependencies:

```
cd /path/to/chromium
gclient sync -D
```

gclient will change V8 to point back to the default google remote, erasing all record/replay changes.  Merge them back in:

```
cd v8
git remote set-url origin https://github.com/replayio/chromium-v8.git
git pull
... fix merge conflicts ...
git commit -a
git push
```

FIXME add instructions for other chromium repositories we've forked.
