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
    if "extends" in tsconfig and tsconfig["extends"]:
        base = path.join(tsconfig_dir, tsconfig["extends"])
        paths.append(base)
        parent = _load(ctx, base, loaded)
        if "files" in parent and not tsconfig["files"]:
            tsconfig["files"] = parent["files"]
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
    if "references" in tsconfig:
        for ref in tsconfig["references"]:
            refname = path.join(path.dir(tsconfig_path), ref["path"])
            if refname not in inputs:
                inputs[refname] = True
            reftc = _load(ctx, refname, loaded)
            for fname in _scan_inputs(ctx, refname, reftc, loaded, scanned):
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
