
This repository is a chromium fork that has has been adapted to use the record/replay driver.

# Setting up builds

Only one build configuration is currently supported.

1. Install chromium tree and depot_tools per https://www.chromium.org/developers/how-tos/get-the-code
   * Warning: Do not use `--no-history` (since it will make it difficult to cast the git re-wiring magic spells below).
1. Change to and pull our (instead of the original) origins for chromium, V8 etc.
   * NOTE: The `gclient sync` here ensures that other third party dependencies are at the right point for our chromium fork, and not upstream tip.
   * NOTE: After `git pull`, you might see "You are not currently on a branch. Please specify which branch you want to merge with.". In this case, `git switch master-or-main` will ignore any merge-related hassles, and instead tracks and switches to the remote `master-or-main` locally ([more info here](https://stackoverflow.com/a/9537923)).
   ```
   cd /path/to/chromium/src
   git remote set-url origin https://github.com/replayio/chromium.git
   git branch -D main
   git pull
   git switch master
   gclient sync
   cd ./v8
   git remote set-url origin https://github.com/replayio/chromium-v8.git
   git branch -D master
   git pull
   git switch master
   cd ../third_party/webrtc
   git remote set-url origin https://github.com/replayio/chromium-webrtc.git
   git branch -D master
   git pull
   git switch main
   cd ../../third_party/skia
   git remote set-url origin https://github.com/replayio/chromium-skia.git
   git branch -D master
   git pull
   git switch main
   ```
1. Setup engflow:
   ```
   export GOMA_SERVER_HOST=simpsonite.goma.engflow.com
   export GOMACTL_USE_PROXY=false
   goma_auth login
   goma_ctl restart
   ```
1. Gen + Configure your build:
   * If you are using Linux, see [Troubleshooting](#troubleshooting) first.
   ```
   cd /path/to/src
   gn gen out/Release
   gn args out/Release # opens args.gn config file
   ```
1. Add the following settings to `args.gn`:
   ```
   # src/out/Release/args.gn

   use_goma = true # if using engflow, false otherwise
   is_debug = false # we can't really use most debugging symbols right now
   enable_nacl = false  # no native targets
   ```
   * On linux add the following settings to disable the allocator shim.
     ```
     use_allocator = "none"
     use_allocator_shim = false
     ```
     * NOTE: This may not be necessary but avoids interactions between the driver and chromium.
   * On linux the following setting may be helpful for looking at stack traces while recording.
     ```
     symbol_level = 1
     ```
     * NOTE: This isn't used for production builds because the resulting binary size is pretty huge.
1. Build:
   ```
   node build
   ```

<<<<<<< HEAD
# Troubleshooting

## Python3 version

When encountering the following errors:
```
ImportError: cannot import name 'Mapping' from 'collections' (/usr/lib/python3.10/collections/__init__.py)
```

On Ubuntu/Debian, make sure Python 3.9 is being used instead of Python 3.10. [Google how](https://www.google.com/search?q=install+python+3.9+ubuntu&hl=en) (the "how" keeps changing), then set it as the system default:

```
<install python3.9>
sudo update-alternatives --install /usr/bin/python3 python3 /usr/bin/python3.9 1 # set 3.9 as the default
```

**WARNING**: On Ubuntu 22.*, this will seriously mess with `apt` and a whole bunch of other things (e.g. the Terminal app won't start from the Gnome app bar). I found that things work fine, if you only use `python3.9` during initial (or maybe some other builds), but is generally not necessary for incremental builds. I thus recommend, using `sudo update-alternatives` in interactive mode to revert that change after the initial full build.

## Missing symlinks

When facing the following:
```
$ gn gen out/Release
ERROR at //build/toolchain/concurrent_links.gni:90:19: Script returned non-zero exit code.
  _command_dict = exec_script("get_concurrent_links.py", _args, "scope")
                  ^----------
```

Try to install some more additional Python-related packages:
```
sudo apt install python-pkg-resources python3-pkg-resources python-is-python3
```

## libssl on Ubuntu 22.04

If launching `out/release/chrome` gives this error message:
```
Loading Record Replay driver failed.
```

This is caused by [missing libssl 1.1 on Ubuntu 22.04](https://askubuntu.com/questions/1424442/libssl1-1-is-deprecated-in-ubuntu-22-04-what-to-do-now), apparently a common problem that breaks many popular software (MongoDB, MariaDB, etc.)

*Solution*: Either stick with Ubuntu 20.04, or (as a workaround) install `libssl 1.1` manually from Ubuntu 20.04:

```
echo "deb http://security.ubuntu.com/ubuntu focal-updates main" | sudo tee /etc/apt/sources.list.d/focal-updates.list
sudo apt update
sudo apt install libssl1.1
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
||||||| 80c960997e61f
For historical reasons, there are some small top level directories. Now the
guidance is that new top level directories are for product (e.g. Chrome,
Android WebView, Ash). Even if these products have multiple executables, the
code should be in subdirectories of the product.
=======
For historical reasons, there are some small top level directories. Now the
guidance is that new top level directories are for product (e.g. Chrome,
Android WebView, Ash). Even if these products have multiple executables, the
code should be in subdirectories of the product.

If you found a bug, please file it at https://crbug.com/new.
>>>>>>> 27d3765d341b09369006d030f83f582a29eb57ae
