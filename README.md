# Malabr

## How to install

Currently on linux is supported.

### Install `depot_tools`

Clone the `depot_tools` repository:

```shell
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
```

Add `depot_tools` to the beginning of your `PATH` (you will probably want to put
this in your `~/.bashrc` or `~/.zshrc`). Assuming you cloned `depot_tools` to
`/path/to/depot_tools`:

```shell
export PATH="/path/to/depot_tools:$PATH"
```

When cloning `depot_tools` to your home directory **do not** use `~` on PATH,
otherwise `gclient runhooks` will fail to run. Rather, you should use either
`$HOME` or the absolute path:

```shell
export PATH="${HOME}/depot_tools:$PATH"
```



### Fetch the Repo

```bash 
mkdir malabr
gclient config --name=src https://github.com/bivas-biswas/malabr.git
gclient sync --no-history
```

### Install additional build dependencies

Once you have checked out the code, and assuming you're using Ubuntu, run
[build/install-build-deps.sh](/build/install-build-deps.sh)

```shell
./build/install-build-deps.sh
```

### Build setup 

```bash
cd src
gclient runhooks
```

###  Configure build using gn

```bash
gn gen out/Default
```

### Build Chromium

```bash
autoninja -C out/Default chrome
```

### Run Chromium

```bash
out/Default/chrome --enable-logging=stderr --v=1
```

