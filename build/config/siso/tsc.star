# -*- bazel-starlark -*-
load("@builtin//encoding.star", "json")
load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")

def _load(ctx, tsconfig_path, loaded):
    if tsconfig_path in loaded:
        return loaded[tsconfig_path]
    tsconfig = json.decode(str(ctx.fs.read(tsconfig_path)))
    loaded[tsconfig_path] = tsconfig
    return tsconfig

def _paths(ctx, tsconfig_path, tsconfig, loaded):
    paths = [tsconfig_path]
    tsconfig_dir = path.dir(tsconfig_path)
    if "files" in tsconfig:
        for file in tsconfig["files"]:
            paths.append(path.join(tsconfig_dir, file))
            if file.endswith(".js"):
                # Add if d.ts version of the file exists.
                file_dts = path.join(tsconfig_dir, file[:-2] + "d.ts")
                if ctx.fs.exists(file_dts):
                    paths.append(file_dts)
    return paths

def _scan_inputs(ctx, tsconfig_path, tsconfig, loaded, scanned):
    if tsconfig_path in scanned:
        return scanned[tsconfig_path]
    inputs = {}
    for fname in _paths(ctx, tsconfig_path, tsconfig, loaded):
        if fname not in inputs:
            inputs[fname] = True
    tsconfig_dir = path.dir(tsconfig_path)
    tsconfig_deps = [ref["path"] for ref in tsconfig.get("references", [])]
    if "extends" in tsconfig:
        tsconfig_deps.append(tsconfig["extends"])
    for tsconfig_dep in tsconfig_deps:
        ref_path = path.join(tsconfig_dir, tsconfig_dep)
        if ref_path not in inputs:
            inputs[ref_path] = True
        ref_tsconfig = _load(ctx, ref_path, loaded)
        for fname in _scan_inputs(ctx, ref_path, ref_tsconfig, loaded, scanned):
            if fname not in inputs:
                inputs[fname] = True
    scanned[tsconfig_path] = inputs.keys()
    return scanned[tsconfig_path]

def _scandeps(ctx, tsconfig_path, tsconfig):
    loaded = {tsconfig_path: tsconfig}
    scanned = {}
    inputs = _scan_inputs(ctx, tsconfig_path, tsconfig, loaded, scanned)
    return inputs

tsc = module(
    "tsc",
    scandeps = _scandeps,
)
