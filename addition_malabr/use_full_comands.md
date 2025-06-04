### PODMAN

build podman

```
podman build -t mycppserver .
```

run podman container

```
podman run -d -p 5000:5000 -v ./uploads:/app/uploads:rw --name cpp-server-container mycppserver;
```

watch the container logs
```
podman logs -f cpp-server-container
```

stop podman container

```
podman stop cpp-server-container
```

remove podman container
```
podman rm cpp-server-container
```

### DOCKER

to list all image

```
docker system df
```

to remove all image
```
docker system prune -af --volumes
```