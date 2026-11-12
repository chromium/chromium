# Key classes

Below is a diagram that shows lifetime and ownership relations between some
of the key classes in the actor component and the `glic` UI that they interact
with.

The diagram is not exhaustive.

```
┌──────────────────────────┐ calls ┌────────────────────────────┐                                                                                                                   
│glic:GlicPageHandler      ┼───────►glic::GlicInstanceImpl      │                                                                                                                   
│1 per GLIC WebUI          │       │N per Profile               │                                                                                                                   
└──┬───────────────────────┘       └──┬─────────────────────────┘                                                                                                                   
   │owns                              │owns                                                                                                                                         
   │                                  │                                                                                                                                             
┌──▼───────────────────────┐       ┌──▼─────────────────────────┐calls  ┌───────────────────┐                                                                                       
│glic::GlicWebClientHandler│       │glic::GlicActorTaskManager  ┼───────►ActorKeyedService  │                                                                                       
│                          ◄─┐     │N per Profile               │     ┌─┐1 per Profile      │                                                                                       
└──┬───────────────────────┘ │calls└────────────────────────────┘     │ └┬┬─────────────────┘                              calls to delegate browser actions                        
   │owns and calls           └────────────────────────────────────────┼──┘│creates and                                    ┌──────────────────────────────────────┐                  
   │                                                                  │   │owns N_task                                    │                                      │                  
┌──▼───────────────────────┐                                          │ ┌─▼─────────────────┐owns 1  ┌────────────────────▼──┐owns 1┌───────────────────┐owns 1┌─┼─────────────────┐
│mojo::Remote<WebClient>   │                                          │ │ActorTask          ┼────────►ExecutionEngine,       ┼──────►ToolController     ┼──────►Tool               │
│Displays GLIC WebUI       │                                          │ │N_task per Profile │        │implements ToolDelegate│      │N_task per Profile │      │N_task per Profile │
└──────────────────────────┘                                          │ └─┬─────────────────┘        │                       │      └───────────────────┘      └─▲─────────────────┘
                                                                      │   │                          │N_task per Profile     ┼────┐                              │                  
                                                                      │   │owns 1                    └─┬──────────────────┬──┘    │owns N_request                │                  
                                                                      │   │                            │                  │       │                              │                  
                                                                      │ ┌─▼─────────────────┐ owns 1   │                  │       │ ┌───────────────────┐ creates│                  
                                                                      │ │UiEventDispatcher  ◄──────────┘                  │       └─►ToolRequest        ┼────────┘                  
                                                                      │ │2N_task per Profile│                             │         │N_task*N_request   │                           
                                                                      │ └─┬─────────────────┘                             │         │per Profile        │                           
                                                                      │   │calls                                          │         └───────────────────┘                           
                                                                owns 1│   │                                               │                                                         
                                                                      │ ┌─▼─────────────────┐                             │                                                         
                                                                      └─►ActorUiStateManager│                             │calls                                                    
                                                                        │1 per Profile      │                             │                                                         
                                                                        └─┬─────────────────┘                             │                                                         
                                                                          │calls                                          │                                                         
                                                                          │                                               │                                                         
                                                                          │                                               │                                                         
                                                                          │                                               │                                                         
                                                                        ┌─▼──────────────────┐       ┌────────────────────▼──┐                                                      
                                                                        │ActorUiTabController│       │General Chrome code    │                                                      
                                                                        │1 per Tab           │       │                       │                                                      
                                                                        └────────────────────┘       └───────────────────────┘                                                      
```

To edit the diagram, copy it into asciiflow.com.

# Headless surfaces: ownership and lifecycle

An `ActorSurface` is a page the actor can act on: either a tab or a headless
page (a `WebContents` with no tab and no window).

* `ActorKeyedService` (1 per profile) is the only API features use to create
  and destroy headless surfaces.
* `HeadlessWebContentsManager` (1 per profile, owned by `ActorKeyedService`)
  owns the headless `WebContents`.
* `ActorSurfaceRegistry` (1 per profile, owned by `ActorKeyedService`) owns
  every `ActorSurface`, tab-backed and headless. Its headless create and
  destroy methods take a `base::PassKey<ActorKeyedService>`.
* `ActorTask` only tracks the surfaces it acts on, by `ActorSurfaceHandle`. It
  never creates or destroys surfaces.

**Creation.** A feature calls `ActorKeyedService::CreateHeadlessActorSurface()`,
typically before the task exists, and passes the surface's handle to the task
(e.g. as the actuation target of a glic invocation). The task picks the surface
up like a tab, through `ActorTask::AddActorSurface()`.

**Teardown is the creator's job.** `CreateHeadlessActorSurface()` returns a
`ScopedHeadlessActorSurface`. Whoever holds it decides how long the page lives:
dropping it destroys the surface and its page, typically after the task acting
on it has completed. This also covers surfaces whose task was never created.
`ActorKeyedService::StopTask()` does not destroy surfaces, and it can't be used
after the task has completed. Surfaces still alive at profile shutdown are
destroyed by `ActorKeyedService::Shutdown()`.

**Not tied to the page or a window.** Unlike a tab, a headless page can't close
itself: `window.close()` is ignored, and the surface and its `WebContents`
stay alive until their owner destroys them.

**Moving between a tab and headless.** A surface keeps its handle and its
`ActorTabData` when its page moves, and ownership of the page moves with it:
* `ActorKeyedService::PromoteHeadlessActorSurface()` takes the page from its
  `ScopedHeadlessActorSurface` and puts it into a new tab; the tab strip owns it
  from then on.
* `ActorKeyedService::DemoteTabToHeadlessActorSurface()` takes a tab's page
  out of the tab strip, closing the tab, and returns a
  `ScopedHeadlessActorSurface` that the caller owns, as if it had created the
  headless page.

**Going away while in use.** If a headless surface is destroyed while a task
still uses it, `ActorKeyedService::DestroyHeadlessActorSurface()` first calls
`ActorTask::OnActorSurfaceWillDetach()` on each task using it, while the page
is still alive. A task added with `stop_task_on_detach` then stops with
`StoppedReason::kTabDetached`. Destroying a surface that is already gone, or
the same surface again while tasks are being told, is a no-op.

```
ScopedHeadlessActorSurface destroyed
 └─► ActorKeyedService::DestroyHeadlessActorSurface()
      ├─► for each task using the surface: ActorTask::OnActorSurfaceWillDetach()
      └─► ActorSurfaceRegistry::DestroyHeadlessActorSurface()
           └─► HeadlessWebContentsManager::Destroy()
```

# Flags

To disable the actor framework in Chrome, pass the following commandline flags:

`--disable-features=GlicActor,GlicActorUi`

