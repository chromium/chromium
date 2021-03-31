
This repository is a chromium fork that has has been adapted to use the record/replay driver.

# Setting up builds

Only one build configuration is currently supported.

Install chromium tree and depot_tools per https://chromium.googlesource.com/chromium/src/+/master/docs/mac_build_instructions.md.

Then change update the remote URLs for chromium and V8 to our forks.

```
cd /path/to/src
git remote set-url origin https://github.com/RecordReplay/chromium.git
git branch -D master
git pull
git checkout master
cd /path/to/src/v8
git remote set-url origin https://github.com/RecordReplay/v8
git branch -D master
git pull
git checkout master
```

Setup the build:

```
cd /path/to/src
gn gen out/Release
gn args out/Release
```

Add the following settings:

```
is_debug = false
enable_nacl = false
```

Build:

```
autoninja -C out/Release chrome
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
git remote set-url origin https://github.com/RecordReplay/v8
git pull
... fix merge conflicts ...
git commit -a
git push
```
