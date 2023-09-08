# -*- bazel-starlark -*-
load("@builtin//encoding.star", "json")
load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")

def _load(ctx, fname, loaded):
    if fname in loaded:
        return loaded[fname]
    tsconfig = json.decode(str(ctx.fs.read(fname)))
    loaded[fname] = tsconfig
    return tsconfig

def _paths(ctx, fname, tsconfig, loaded):
    paths = [fname]
    if "extends" in tsconfig and tsconfig["extends"]:
        base = path.join(path.dir(fname), tsconfig["extends"])
        paths.append(base)
        parent = _load(ctx, base, loaded)
        if "files" in parent and not tsconfig["files"]:
            tsconfig["files"] = parent["files"]
    paths.extend([path.join(path.dir(fname), file) for file in tsconfig["files"]])
    return paths

def _scan_inputs(ctx, fname, tsconfig, loaded):
    inputs = []
    inputs += _paths(ctx, fname, tsconfig, loaded)
    if "references" in tsconfig:
        for ref in tsconfig["references"]:
            refname = path.join(path.dir(fname), ref["path"])
            inputs.append(refname)
            reftc = _load(ctx, refname, loaded)
            inputs += _scan_inputs(ctx, refname, reftc, loaded)
    return inputs

def _scandeps(ctx, fname, tsconfig):
    loaded = {fname: tsconfig}
    inputs = _scan_inputs(ctx, fname, tsconfig, loaded)
    return inputs

tsc = module(
    "tsc",
    scandeps = _scandeps,
)
